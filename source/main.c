#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/dialog.h"
#include "utils/logger.h"

#include <psp2/kernel/threadmgr.h>

#include <falso_jni/FalsoJNI.h>
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

static bg2_jni_on_load_fn require_jni_on_load(void) {
    bg2_jni_on_load_fn entry =
        (bg2_jni_on_load_fn)so_symbol(&so_mod, "JNI_OnLoad");
    if (entry == NULL) {
        fatal_error("BG2V milestone failed: JNI_OnLoad was not exported by %s.",
                    SO_PATH);
    }
    return entry;
}

int main() {
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
    /*
     * Deliberate proof-of-concept stop. The next milestone will provide the
     * SDLActivity Java callbacks and invoke nativeInit on its own thread.
     * Stopping visibly here distinguishes a successful loader/JNI bootstrap
     * from a black-screen hang.
     */
    fatal_error("BG2V loader milestone passed.\n\n"
                "libBaldursGate.so loaded, imports resolved, JNI_OnLoad "
                "returned 0x%x, and VitaGL initialized.", jni_version);
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
