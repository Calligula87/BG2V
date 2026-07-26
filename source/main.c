#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/dialog.h"
#include "utils/logger.h"

#include <psp2/kernel/threadmgr.h>

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

enum bg2_bootstrap_state {
    BG2_BOOTSTRAP_NOT_STARTED = 0,
    BG2_BOOTSTRAP_ENTERED = 1,
    BG2_BOOTSTRAP_RETURNED = 2,
};

static volatile int bootstrap_state = BG2_BOOTSTRAP_NOT_STARTED;
static volatile int bootstrap_result = 0;
static bg2_sdl_native_init_fn SDLActivity_nativeInit;

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

    int elapsed_ms = 0;
    int milestone_logged = 0;
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
        sceKernelDelayThread(100 * 1000);
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
    // Call into the .so here
}

void controls_handler_touch(int32_t id, float x, float y, ControlsAction action) {
    // Call into the .so here
}

void controls_handler_analog(ControlsStickId which, float x, float y, ControlsAction action) {
    // Call into the .so here
}
#endif
