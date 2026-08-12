#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/dialog.h"
#include "utils/logger.h"

#include <psp2/kernel/threadmgr.h>
#include <string.h>

#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_ImplBridge.h>
#include <so_util/so_util.h>

#ifndef NDK_PORT
#include "reimpl/controls.h"
#else
#include <falso_ndk/FalsoNDK.h>
#endif

/*
 * The verified AREA060A ETC2 run used about 120 MiB of CPU heap and left at
 * least 62 MiB free in vitaGL's 227 MiB pool. Reserve 176 MiB for the engine
 * while retaining enough GPU-side memory for decoded area textures.
 */
int _newlib_heap_size_user = 176 * 1024 * 1024;

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 4 * 1024 * 1024;
#endif

so_module so_mod;


typedef int (*bg2_jni_on_load_fn)(void *jvm);
typedef jint (*bg2_sdl_native_init_fn)(
    JNIEnv *env, jclass activity_class, jobject arguments);
typedef void (*bg2_sdl_native_key_fn)(
    JNIEnv *env, jclass activity_class, jint keycode);
typedef void (*bg2_sdl_native_touch_fn)(
    JNIEnv *env, jclass activity_class, jint device_id, jint finger_id,
    jint action, jfloat x, jfloat y, jfloat pressure);
typedef void (*bg2_sdl_native_resize_fn)(
    JNIEnv *env, jclass activity_class, jint width, jint height,
    jint format, jfloat refresh_rate);
typedef void (*bg2_sdl_native_surface_fn)(
    JNIEnv *env, jclass activity_class);
typedef void (*bg2_sdl_native_mouse_fn)(
    JNIEnv *env, jclass activity_class, jint button, jint action,
    jfloat x, jfloat y);
typedef void (*bg2_sdl_native_pan_fn)(
    JNIEnv *env, jclass activity_class, jfloat delta_x, jfloat delta_y);
typedef void (*bg2_sdl_native_commit_text_fn)(
    JNIEnv *env, jobject input_connection, jstring text, jint new_cursor);
typedef int (*bg2_sdl_send_keyboard_text_fn)(const char *text);
typedef int (*bg2_sdl_send_editing_text_fn)(
    const char *text, int start, int length);
typedef void (*bg2_sdl_start_text_input_fn)(void);

enum bg2_bootstrap_state {
    BG2_BOOTSTRAP_NOT_STARTED = 0,
    BG2_BOOTSTRAP_ENTERED = 1,
    BG2_BOOTSTRAP_RETURNED = 2,
};

static volatile int bootstrap_state = BG2_BOOTSTRAP_NOT_STARTED;
static volatile int bootstrap_result = 0;
static bg2_sdl_native_init_fn SDLActivity_nativeInit;
static bg2_sdl_native_key_fn SDLActivity_onNativeKeyDown;
static bg2_sdl_native_key_fn SDLActivity_onNativeKeyUp;
static bg2_sdl_native_touch_fn SDLActivity_onNativeTouch;
static bg2_sdl_native_resize_fn SDLActivity_onNativeResize;
static bg2_sdl_native_surface_fn SDLActivity_onNativeSurfaceChanged;
static bg2_sdl_native_mouse_fn SDLActivity_onNativeMouse;
static bg2_sdl_native_pan_fn SDLActivity_onNativePan;
static bg2_sdl_native_commit_text_fn SDLInputConnection_nativeCommitText;
static bg2_sdl_send_keyboard_text_fn SDL_SendKeyboardText_internal;
static bg2_sdl_send_editing_text_fn SDL_SendEditingText_internal;
static bg2_sdl_start_text_input_fn SDL_StartTextInput;
static float pointer_x = 480.0f;
static float pointer_y = 272.0f;
static unsigned int pointer_button_state;
static int pointer_drag_active;
static int native_touch_moved;
static int native_touch_active_id = -1;
static float native_touch_down_x;
static float native_touch_down_y;
static float native_touch_last_x;
static float native_touch_last_y;
static int native_selection_armed;
static int native_touch_selection_drag;

#define BG2V_TOUCH_SLOP_PIXELS 16.0f
#define BG2V_TOUCH_PAN_GAIN 1.5f
#define BG2V_TOUCH_MAX_PAN_STEP 36.0f
#define BG2V_SELECTION_BUTTON_MIN_X 880.0f
#define BG2V_SELECTION_BUTTON_MIN_Y 380.0f
#define BG2V_SELECTION_BUTTON_MAX_Y 470.0f

#define BG2V_NAME_FIELD_X 480.0f
#define BG2V_NAME_FIELD_Y 215.0f

extern int bg2v_take_text_input_request(void);
extern void bg2v_finish_text_input(void);
extern void bg2v_queue_chargen_name(const char *name);
extern volatile int bg2v_selection_enabled;

static void bg2v_refocus_text_field(void) {
    /*
     * Opening the Vita common-dialog IME makes BG2 stop SDL text input and
     * release its edit-box focus. The click which opened the name popup is on
     * the left-hand NAME row, not the edit box created inside the popup. Click
     * the known centre of that edit box after the dialog has closed. Keeping
     * text_input_busy set suppresses the second Vita keyboard request.
     */
    SDLActivity_onNativeMouse(
        &jni, (jclass)0x42424242, 0, 7,
        BG2V_NAME_FIELD_X, BG2V_NAME_FIELD_Y);
    SDLActivity_onNativeMouse(
        &jni, (jclass)0x42424242, 1, 0,
        BG2V_NAME_FIELD_X, BG2V_NAME_FIELD_Y);
    SDLActivity_onNativeMouse(
        &jni, (jclass)0x42424242, 1, 1,
        BG2V_NAME_FIELD_X, BG2V_NAME_FIELD_Y);
    bg2v_log_printf(
        "[BG2V][IME] name popup opened at %.0f,%.0f; "
        "focused edit box at %.0f,%.0f\n",
        pointer_x, pointer_y, BG2V_NAME_FIELD_X, BG2V_NAME_FIELD_Y);
}

static bg2_jni_on_load_fn require_jni_on_load(void) {
    bg2_jni_on_load_fn entry =
        (bg2_jni_on_load_fn)so_symbol(&so_mod, "JNI_OnLoad");
    if (entry == NULL) {
        fatal_error("BG2V milestone failed: JNI_OnLoad was not exported by %s.",
                    SO_PATH);
    }
    return entry;
}

static bg2_sdl_native_init_fn require_sdl_native_init(void) {
    bg2_sdl_native_init_fn entry = (bg2_sdl_native_init_fn)so_symbol(
        &so_mod, "Java_org_libsdl_app_SDLActivity_nativeInit");
    if (entry == NULL) {
        fatal_error("BG2V milestone failed: SDLActivity.nativeInit was not "
                    "exported.");
    }
    return entry;
}

static void require_sdl_input_bridges(void) {
    SDLActivity_onNativeKeyDown = (bg2_sdl_native_key_fn)so_symbol(
        &so_mod, "Java_org_libsdl_app_SDLActivity_onNativeKeyDown");
    SDLActivity_onNativeKeyUp = (bg2_sdl_native_key_fn)so_symbol(
        &so_mod, "Java_org_libsdl_app_SDLActivity_onNativeKeyUp");
    SDLActivity_onNativeTouch = (bg2_sdl_native_touch_fn)so_symbol(
        &so_mod, "Java_org_libsdl_app_SDLActivity_onNativeTouch");
    SDLActivity_onNativeResize = (bg2_sdl_native_resize_fn)so_symbol(
        &so_mod, "Java_org_libsdl_app_SDLActivity_onNativeResize");
    SDLActivity_onNativeSurfaceChanged =
        (bg2_sdl_native_surface_fn)so_symbol(
            &so_mod,
            "Java_org_libsdl_app_SDLActivity_onNativeSurfaceChanged");
    SDLActivity_onNativeMouse = (bg2_sdl_native_mouse_fn)so_symbol(
        &so_mod, "Java_org_libsdl_app_SDLActivity_onNativeMouse");
    SDLActivity_onNativePan = (bg2_sdl_native_pan_fn)so_symbol(
        &so_mod, "Java_org_libsdl_app_SDLActivity_onNativePan");
    SDLInputConnection_nativeCommitText =
        (bg2_sdl_native_commit_text_fn)so_symbol(
            &so_mod,
            "Java_org_libsdl_app_SDLInputConnection_nativeCommitText");
    SDL_StartTextInput = (bg2_sdl_start_text_input_fn)so_symbol(
        &so_mod, "SDL_StartTextInput");

    /*
     * The bundled SDL keeps SDL_SendKeyboardText private. Its JNI commit
     * wrapper is exported and calls that helper at a stable relative offset
     * in BG2EE 2.6.6.13. Calling the helper directly returns whether SDL
     * actually queued the text event, which makes Vita IME delivery
     * observable and avoids a second fake-JNI string conversion.
     */
    uintptr_t commit_entry =
        (uintptr_t)SDLInputConnection_nativeCommitText & ~(uintptr_t)1;
    SDL_SendKeyboardText_internal =
        (bg2_sdl_send_keyboard_text_fn)(commit_entry - 0x266c4);
    SDL_SendEditingText_internal =
        (bg2_sdl_send_editing_text_fn)(commit_entry - 0x26630);

    if (!SDLActivity_onNativeKeyDown || !SDLActivity_onNativeKeyUp ||
        !SDLActivity_onNativeTouch || !SDLActivity_onNativeResize ||
        !SDLActivity_onNativeSurfaceChanged || !SDLActivity_onNativeMouse ||
        !SDLActivity_onNativePan ||
        !SDLInputConnection_nativeCommitText ||
        !SDL_SendKeyboardText_internal || !SDL_SendEditingText_internal ||
        !SDL_StartTextInput) {
        fatal_error("BG2V could not resolve SDL's native activity callbacks.");
    }
    bg2v_log_printf("[BG2V] SDL input and surface bridges resolved\n");
}

static jobject make_sdl_arguments(void) {
    jstring empty_argument = jni->NewStringUTF(&jni, "");
    if (empty_argument == NULL) {
        fatal_error("BG2V could not create the SDL argument string.");
    }

    jobjectArray arguments = jni->NewObjectArray(
        &jni, 1, (jclass)0x42424242, empty_argument);
    if (arguments == NULL) {
        fatal_error("BG2V could not create the SDL argument array.");
    }
    return arguments;
}

static int bg2_sdl_thread(SceSize args, void *argp) {
    (void)args;
    jobject arguments = *(jobject *)argp;
    bootstrap_state = BG2_BOOTSTRAP_ENTERED;
    bg2v_log_printf("[BG2V] Entering SDLActivity.nativeInit\n");
    bootstrap_result = SDLActivity_nativeInit(
        &jni, (jclass)0x42424242, arguments);
    bootstrap_state = BG2_BOOTSTRAP_RETURNED;
    bg2v_log_printf("[BG2V] SDLActivity.nativeInit returned %d\n",
                    bootstrap_result);
    return 0;
}

int main() {
    bg2v_log_reset();
    soloader_init_all();

    bg2_jni_on_load_fn JNI_OnLoad = require_jni_on_load();
    int jni_version = JNI_OnLoad(&jvm);
    if (jni_version == 0) {
        fatal_error("BG2V reached JNI_OnLoad, but the engine rejected the "
                    "fake Java VM.");
    }
    l_success("BG2 JNI_OnLoad returned 0x%x.", jni_version);

    gl_init();

#ifndef NDK_PORT
    SDLActivity_nativeInit = require_sdl_native_init();
    require_sdl_input_bridges();
    jobject arguments = make_sdl_arguments();
    SceUID thread = sceKernelCreateThread(
        "bg2v_sdl_main", bg2_sdl_thread, 0x10000100,
        1024 * 1024, 0, 0, NULL);
    if (thread < 0) {
        fatal_error("BG2V could not create the SDL thread (0x%08x).", thread);
    }
    int start_result = sceKernelStartThread(
        thread, sizeof(arguments), &arguments);
    if (start_result < 0) {
        fatal_error("BG2V could not start the SDL thread (0x%08x).",
                    start_result);
    }

    /*
     * Android normally delivers these callbacks from its UI thread shortly
     * after nativeInit starts. Without them SDL retains a 0x0 display size;
     * BG2 then divides its render coordinates by zero.
     */
    sceKernelDelayThread(250 * 1000);
    SDLActivity_onNativeResize(
        &jni, (jclass)0x42424242, 960, 544, 1, 60.0f);
    SDLActivity_onNativeSurfaceChanged(&jni, (jclass)0x42424242);
    bg2v_log_printf(
        "[BG2V] SDL surface announced: 960x544 RGBA8888 @ 60 Hz\n");

    int elapsed_ms = 0;
    int milestone_logged = 0;
    int ime_active = 0;
    int ime_delivery_phase = 0;
    int ime_delivery_delay = 0;
    char ime_pending_text[32] = "";
    for (;;) {
        if (bootstrap_state == BG2_BOOTSTRAP_RETURNED) {
            fatal_error("SDLActivity.nativeInit returned %d.\n\n"
                        "Please send ux0:data/bg2v/bootstrap.log.",
                        bootstrap_result);
        }
        if (!milestone_logged) {
            elapsed_ms += 100;
            if (elapsed_ms >= 15000) {
                l_success("SDLActivity.nativeInit stayed active for 15 "
                          "seconds; continuing without watchdog.");
                milestone_logged = 1;
            }
        }
        if (!ime_active && ime_delivery_phase == 0 &&
            bg2v_take_text_input_request()) {
            int result = init_ime_dialog("Character name", "");
            if (result >= 0) {
                ime_active = 1;
                bg2v_log_printf("[BG2V][IME] Vita keyboard opened\n");
            } else {
                bg2v_log_printf(
                    "[BG2V][IME] keyboard open failed: 0x%08x\n", result);
                bg2v_finish_text_input();
            }
        }

        if (ime_active) {
            char *text = get_ime_dialog_result();
            if (text != NULL) {
                strncpy(
                    ime_pending_text, text, sizeof(ime_pending_text) - 1);
                ime_pending_text[sizeof(ime_pending_text) - 1] = '\0';
                bg2v_log_printf(
                    "[BG2V][IME] keyboard closed, retained %u bytes\n",
                    (unsigned int)strlen(ime_pending_text));
                ime_active = 0;

                if (ime_pending_text[0] != '\0') {
                    /*
                     * Give the common dialog a few frames to leave, refocus
                     * the field, then give BG2 time to consume the synthetic
                     * click before committing the retained string.
                     */
                    ime_delivery_phase = 1;
                    ime_delivery_delay = 4;
                } else {
                    bg2v_finish_text_input();
                }
            }
        } else if (ime_delivery_phase != 0) {
            if (ime_delivery_delay > 0) {
                --ime_delivery_delay;
            } else if (ime_delivery_phase == 1) {
                bg2v_refocus_text_field();
                ime_delivery_phase = 2;
                ime_delivery_delay = 12;
            } else {
                /*
                 * The engine polls SDL_TEXTINPUT but its UI edit widget does
                 * not consume it in this Android-free bootstrap. Queue the
                 * retained value for assignment to UI.MENU's charNameEdit
                 * variable from the next Lua call on the engine thread.
                 */
                bg2v_queue_chargen_name(ime_pending_text);
                ime_pending_text[0] = '\0';
                ime_delivery_phase = 0;
                bg2v_finish_text_input();
            }
        } else {
            controls_poll();
        }
        sceKernelDelayThread(16 * 1000);
    }
#else
    // Build a fake ANativeActivity that the game's onCreate will receive
    ANativeActivity *activity = malloc(sizeof(ANativeActivity));
    activity->callbacks = malloc(sizeof(ANativeActivityCallbacks));
    activity->env = &jni; // from FalsoJNI
    activity->vm = &jvm;  // from FalsoJNI
    activity->clazz = (jclass)0x42424242;
    activity->internalDataPath = DATA_PATH "assets/";
    activity->externalDataPath = DATA_PATH "assets/";
    activity->sdkVersion = 14;
    activity->instance = NULL;

    // Drive the activity lifecycle
    int (*ANativeActivity_onCreate)(ANativeActivity *, void *, size_t) =
        (void *)so_symbol(&so_mod, "ANativeActivity_onCreate");
    ANativeActivity_onCreate(activity, NULL, 0);

    activity->callbacks->onStart(activity);
    activity->callbacks->onResume(activity);

    // Wire up input and the native window
    AInputQueue *aInputQueue = AInputQueue_create();
    activity->callbacks->onInputQueueCreated(activity, aInputQueue);

    ANativeWindow *aNativeWindow = ANativeWindow_create();
    activity->callbacks->onNativeWindowCreated(activity, aNativeWindow);

    activity->callbacks->onWindowFocusChanged(activity, 1);
#endif

    sceKernelExitDeleteThread(0);
}

#ifndef NDK_PORT
void controls_handler_key(int32_t keycode, ControlsAction action) {
    if (action == CONTROLS_ACTION_DOWN) {
        SDLActivity_onNativeKeyDown(&jni, (jclass)0x42424242, keycode);
    } else if (action == CONTROLS_ACTION_UP) {
        SDLActivity_onNativeKeyUp(&jni, (jclass)0x42424242, keycode);
    }
}

void controls_handler_touch(int32_t id, float x, float y, ControlsAction action) {
    /*
     * Preserve the most recent touch location as well as analog-pointer
     * location. The IME refocus sequence needs the exact box the user tapped,
     * and keeping the software pointer visible makes touch and stick input
     * share one predictable cursor position.
     */
    pointer_x = x;
    pointer_y = y;
    gl_pointer_set(pointer_x, pointer_y, GL_TRUE);
    jint android_action;
    switch (action) {
    case CONTROLS_ACTION_DOWN:
        android_action = 0; /* MotionEvent.ACTION_DOWN */
        native_touch_moved = 0;
        native_touch_active_id = id;
        native_touch_selection_drag = native_selection_armed;
        native_selection_armed = 0;
        native_touch_down_x = x;
        native_touch_down_y = y;
        native_touch_last_x = x;
        native_touch_last_y = y;
        bg2v_log_printf(
            "[BG2V][TOUCH] down id=%d at %.0f,%.0f mode=%s\n",
            id, x, y,
            native_touch_selection_drag ? "selection" : "normal");
        break;
    case CONTROLS_ACTION_UP:
        android_action = 1; /* MotionEvent.ACTION_UP */
        bg2v_log_printf(
            "[BG2V][TOUCH] up id=%d at %.0f,%.0f moved=%d\n",
            id, x, y, native_touch_moved);
        if (native_touch_active_id == id) {
            native_touch_active_id = -1;
        }
        if (!native_touch_moved &&
            x >= BG2V_SELECTION_BUTTON_MIN_X &&
            y >= BG2V_SELECTION_BUTTON_MIN_Y &&
            y <= BG2V_SELECTION_BUTTON_MAX_Y) {
            native_selection_armed = 1;
            bg2v_log_printf(
                "[BG2V][TOUCH] selection gesture armed at %.0f,%.0f\n",
                x, y);
        }
        break;
    default:
        android_action = 2; /* MotionEvent.ACTION_MOVE */
        if (native_touch_active_id == id) {
            float delta_x;
            float delta_y;

            if (!native_touch_moved) {
                /*
                 * Android's GestureDetector suppresses sub-touch-slop motion.
                 * Preserve the DOWN position until the gesture crosses that
                 * threshold so a slow drag does not begin as a series of
                 * one-pixel selection boxes.
                 */
                delta_x = x - native_touch_down_x;
                delta_y = y - native_touch_down_y;
                if ((delta_x * delta_x) + (delta_y * delta_y) <
                    BG2V_TOUCH_SLOP_PIXELS * BG2V_TOUCH_SLOP_PIXELS) {
                    break;
                }

                if (!bg2v_selection_enabled) {
                    /*
                     * For ordinary map dragging, swallowing the first motion
                     * frame after touch-slop avoids a noticeable "jump" where
                     * the camera consumes the whole accumulated down-to-move
                     * distance at once.
                     */
                    native_touch_moved = 1;
                    native_touch_last_x = x;
                    native_touch_last_y = y;
                    bg2v_log_printf(
                        "[BG2V][TOUCH] drag begin id=%d at %.0f,%.0f "
                        "selection=0\n",
                        id, x, y);
                    break;
                }
            } else if (native_touch_selection_drag) {
                /*
                 * The game's selection rectangle consumes an absolute extent
                 * from the original touch point. This is intentionally used
                 * only for the first drag after tapping the selection button;
                 * applying it to ordinary map movement causes accumulation.
                 */
                delta_x = x - native_touch_down_x;
                delta_y = y - native_touch_down_y;
            } else {
                /*
                 * SetSelectionEnabled remains true throughout normal BG2
                 * gameplay, not only while the selection tool is active.
                 * Re-sending the full DOWN-to-current displacement on every
                 * frame therefore accumulated exponentially and made short
                 * touches fling the map.  Send only the latest finger step,
                 * with a modest gain so deliberate selection rectangles still
                 * track comfortably across the Vita screen.
                 */
                delta_x = x - native_touch_last_x;
                delta_y = y - native_touch_last_y;
            }

            if (delta_x != 0.0f || delta_y != 0.0f) {
                if (!native_touch_selection_drag) {
                    delta_x *= BG2V_TOUCH_PAN_GAIN;
                    delta_y *= BG2V_TOUCH_PAN_GAIN;
                    if (delta_x > BG2V_TOUCH_MAX_PAN_STEP)
                        delta_x = BG2V_TOUCH_MAX_PAN_STEP;
                    if (delta_x < -BG2V_TOUCH_MAX_PAN_STEP)
                        delta_x = -BG2V_TOUCH_MAX_PAN_STEP;
                    if (delta_y > BG2V_TOUCH_MAX_PAN_STEP)
                        delta_y = BG2V_TOUCH_MAX_PAN_STEP;
                    if (delta_y < -BG2V_TOUCH_MAX_PAN_STEP)
                        delta_y = -BG2V_TOUCH_MAX_PAN_STEP;
                }
                native_touch_last_x = x;
                native_touch_last_y = y;
                SDLActivity_onNativePan(
                    &jni, (jclass)0x42424242, delta_x, delta_y);
                if (!native_touch_moved) {
                    native_touch_moved = 1;
                    bg2v_log_printf(
                        "[BG2V][TOUCH] native pan id=%d delta=%.1f,%.1f "
                        "at %.0f,%.0f selection=%d\n",
                        id, delta_x, delta_y, x, y,
                        native_touch_selection_drag);
                }
            }
        }
        break;
    }

    SDLActivity_onNativeTouch(
        &jni, (jclass)0x42424242, 0, id, android_action,
        x / 960.0f, y / 544.0f, 1.0f);
}

void controls_handler_analog(ControlsStickId which, float x, float y, ControlsAction action) {
    if (which != CONTROLS_STICK_LEFT || action == CONTROLS_ACTION_UP)
        return;
    if (x == 0.0f && y == 0.0f)
        return;

    pointer_x += x * 9.0f;
    pointer_y += y * 9.0f;
    if (pointer_x < 0.0f) pointer_x = 0.0f;
    if (pointer_y < 0.0f) pointer_y = 0.0f;
    if (pointer_x > 959.0f) pointer_x = 959.0f;
    if (pointer_y > 543.0f) pointer_y = 543.0f;
    gl_pointer_set(pointer_x, pointer_y, GL_TRUE);

    /*
     * A held Android mouse button must accompany ACTION_MOVE for Beamdog's
     * SDL fork to retain drag state. HOVER_MOVE is reserved for movement with
     * no buttons down. This is what lets BG2 draw its party-selection box
     * while Cross is held and the left stick moves.
     */
    jint motion_action =
        pointer_button_state != 0 ? 2 : 7; /* ACTION_MOVE / HOVER_MOVE */
    if (pointer_button_state != 0 && !pointer_drag_active) {
        pointer_drag_active = 1;
        bg2v_log_printf(
            "[BG2V][INPUT] pointer drag begin buttons=0x%x at %.0f,%.0f\n",
            pointer_button_state, pointer_x, pointer_y);
    }
    SDLActivity_onNativeMouse(
        &jni, (jclass)0x42424242, (jint)pointer_button_state,
        motion_action, pointer_x, pointer_y);
}

void controls_handler_pointer_button(int32_t button, ControlsAction action) {
    jint android_action =
        action == CONTROLS_ACTION_DOWN ? 0 : 1; /* ACTION_DOWN / ACTION_UP */

    if (action == CONTROLS_ACTION_DOWN) {
        pointer_button_state |= (unsigned int)button;
        pointer_drag_active = 0;
    }

    /* Position the Android mouse before changing the requested button. */
    SDLActivity_onNativeMouse(
        &jni, (jclass)0x42424242, 0,
        7, pointer_x, pointer_y);

    /*
     * onNativeMouse's first integer is the button which changed, not the
     * post-event button-state mask.  In particular ACTION_UP must still carry
     * button 1; clearing the mask first sent a bogus "button 0 released" and
     * left Beamdog's selection gesture unfinished.
     */
    SDLActivity_onNativeMouse(
        &jni, (jclass)0x42424242, (jint)button,
        android_action,
        pointer_x, pointer_y);

    if (action == CONTROLS_ACTION_UP) {
        pointer_button_state &= ~(unsigned int)button;
    }

    if (action == CONTROLS_ACTION_UP && pointer_drag_active) {
        bg2v_log_printf(
            "[BG2V][INPUT] pointer drag end at %.0f,%.0f\n",
            pointer_x, pointer_y);
        pointer_drag_active = 0;
    }
}

void controls_handler_scroll(float y) {
    /*
     * Android MotionEvent.ACTION_SCROLL is translated by BG2's SDL layer
     * into an SDL mouse-wheel event. The game's touch-first menus ignore
     * keyboard Page Up/Page Down, but consume this path for scrollable lists
     * and text panels.
     */
    SDLActivity_onNativeMouse(
        &jni, (jclass)0x42424242, 0,
        8, /* MotionEvent.ACTION_SCROLL */
        0.0f, y);
    bg2v_log_printf("[BG2V][INPUT] scroll y=%.1f\n", y);
}
#endif
