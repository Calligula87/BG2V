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

int _newlib_heap_size_user = 256 * 1024 * 1024;

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
static bg2_sdl_native_commit_text_fn SDLInputConnection_nativeCommitText;
static bg2_sdl_send_keyboard_text_fn SDL_SendKeyboardText_internal;
static bg2_sdl_send_editing_text_fn SDL_SendEditingText_internal;
static bg2_sdl_start_text_input_fn SDL_StartTextInput;
static float pointer_x = 480.0f;
static float pointer_y = 272.0f;

extern int bg2v_take_text_input_request(void);
extern void bg2v_finish_text_input(void);

static void bg2v_refocus_text_field(void) {
    /*
     * Opening the Vita common-dialog IME makes BG2 stop SDL text input and
     * release its edit-box focus. Replaying the activation click after the
     * dialog has closed lets BG2 rebuild that state before we commit text.
     * text_input_busy remains set, so the replayed click cannot open a second
     * Vita keyboard.
     */
    SDLActivity_onNativeMouse(
        &jni, (jclass)0x42424242, 0, 7, pointer_x, pointer_y);
    SDLActivity_onNativeMouse(
        &jni, (jclass)0x42424242, 1, 0, pointer_x, pointer_y);
    SDLActivity_onNativeMouse(
        &jni, (jclass)0x42424242, 1, 1, pointer_x, pointer_y);
    bg2v_log_printf(
        "[BG2V][IME] replayed focus click at %.0f,%.0f\n",
        pointer_x, pointer_y);
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
                int editing_queued;
                int text_queued;

                SDL_StartTextInput();
                editing_queued = SDL_SendEditingText_internal(
                    ime_pending_text, 0, 0);
                text_queued =
                    SDL_SendKeyboardText_internal(ime_pending_text);
                bg2v_log_printf(
                    "[BG2V][IME] refocused commit of %u bytes, "
                    "editing queued=%d, text queued=%d\n",
                    (unsigned int)strlen(ime_pending_text),
                    editing_queued, text_queued);
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
     * location. The IME refocus sequence needs the exact box the user tapped.
     */
    pointer_x = x;
    pointer_y = y;
    gl_pointer_set(pointer_x, pointer_y, GL_FALSE);
    jint android_action;
    switch (action) {
    case CONTROLS_ACTION_DOWN:
        android_action = 0; /* MotionEvent.ACTION_DOWN */
        break;
    case CONTROLS_ACTION_UP:
        android_action = 1; /* MotionEvent.ACTION_UP */
        break;
    default:
        android_action = 2; /* MotionEvent.ACTION_MOVE */
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

    /* MotionEvent.ACTION_HOVER_MOVE */
    SDLActivity_onNativeMouse(
        &jni, (jclass)0x42424242, 0, 7, pointer_x, pointer_y);
}

void controls_handler_pointer_button(int32_t button, ControlsAction action) {
    jint android_action =
        action == CONTROLS_ACTION_DOWN ? 0 : 1; /* ACTION_DOWN / ACTION_UP */
    SDLActivity_onNativeMouse(
        &jni, (jclass)0x42424242, 0, 7, pointer_x, pointer_y);
    SDLActivity_onNativeMouse(
        &jni, (jclass)0x42424242, button, android_action,
        pointer_x, pointer_y);
}
#endif
