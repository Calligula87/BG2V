#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>

#include "utils/logger.h"

/*
 * JNI Methods
*/

enum bg2_java_method_id {
	METHOD_GET_NATIVE_SURFACE = 100,
	METHOD_AUDIO_INIT,
	METHOD_AUDIO_WRITE_SHORT_BUFFER,
	METHOD_AUDIO_WRITE_BYTE_BUFFER,
	METHOD_AUDIO_QUIT,
	METHOD_POLL_INPUT_DEVICES,
	METHOD_INPUT_GET_INPUT_DEVICE_IDS,
	METHOD_SEND_MESSAGE,
	METHOD_GET_CONTEXT,
	METHOD_GET_APK_PATH,
	METHOD_GET_EXTERNAL_STORAGE_STATE,
	METHOD_GET_FILES_DIR,
	METHOD_GET_ABSOLUTE_PATH,
	METHOD_GET_EXTERNAL_FILES_DIR,
	METHOD_IS_WIFI_ON,
	METHOD_WRITE_TO_LOG,
	METHOD_GET_LANGUAGE_STRING,
	METHOD_SHOW_TEXT_INPUT,
};

static volatile int text_input_requested;
static volatile int text_input_x;
static volatile int text_input_y;
static volatile int text_input_w;
static volatile int text_input_h;

int bg2v_take_text_input_request(void) {
	return __sync_bool_compare_and_swap(&text_input_requested, 1, 0);
}

static jboolean showTextInput(jmethodID id, va_list args) {
	(void)id;
	text_input_x = va_arg(args, jint);
	text_input_y = va_arg(args, jint);
	text_input_w = va_arg(args, jint);
	text_input_h = va_arg(args, jint);
	__sync_synchronize();
	text_input_requested = 1;
	bg2v_log_printf(
		"[BG2V][JNI] showTextInput rect=%d,%d %dx%d\n",
		text_input_x, text_input_y, text_input_w, text_input_h);
	return JNI_TRUE;
}

static jobject getNativeSurface(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	/* Stable non-null token; VitaGL owns the real display surface. */
	bg2v_log_printf("[BG2V][JNI] getNativeSurface\n");
	return (jobject)0x42420001;
}

static jint audioInit(jmethodID id, va_list args) {
	(void)id;
	jint sample_rate = va_arg(args, jint);
	jboolean is_16_bit = va_arg(args, jint);
	jboolean is_stereo = va_arg(args, jint);
	jint desired_frames = va_arg(args, jint);
	bg2v_log_printf(
		"[BG2V][JNI] audioInit rate=%d 16bit=%d stereo=%d frames=%d\n",
		sample_rate, is_16_bit, is_stereo, desired_frames);
	/*
	 * Report success for the bootstrap probe. The Java AudioTrack path is
	 * intentionally silent until it is bridged to SceAudioOut; BG2's OpenAL
	 * path is already mapped separately.
	 */
	return 0;
}

static void audioWriteShortBuffer(jmethodID id, va_list args) {
	(void)id;
	(void)va_arg(args, jshortArray);
}

static void audioWriteByteBuffer(jmethodID id, va_list args) {
	(void)id;
	(void)va_arg(args, jbyteArray);
}

static void audioQuit(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	bg2v_log_printf("[BG2V][JNI] audioQuit\n");
}

static void pollInputDevices(jmethodID id, va_list args) {
	(void)id;
	(void)args;
}

static jobject inputGetInputDeviceIds(jmethodID id, va_list args) {
	(void)id;
	jint sources = va_arg(args, jint);
	bg2v_log_printf(
		"[BG2V][JNI] inputGetInputDeviceIds sources=0x%x (empty)\n",
		(unsigned int)sources);
	return jni->NewIntArray(&jni, 0);
}

static jboolean sendMessage(jmethodID id, va_list args) {
	(void)id;
	jint command = va_arg(args, jint);
	jint parameter = va_arg(args, jint);
	bg2v_log_printf(
		"[BG2V][JNI] sendMessage command=%d parameter=%d\n",
		command, parameter);
	/* SDL uses this for optional Android UI commands; acknowledge them. */
	return JNI_TRUE;
}

static jobject getContext(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	/* Stable non-null token for code which only tests for a Context object. */
	return (jobject)0x42420002;
}

static jobject getAPKPath(jmethodID id, va_list args) {
	(void)id;
	jint path_kind = va_arg(args, jint);
	const char *path;

	switch (path_kind) {
	case -2:
		/* Android package-code path; kept local to the user data directory. */
		path = DATA_PATH "game.apk";
		break;
	case -1:
		/* OBB directory with the trailing slash used by Beamdog's Java code. */
		path = DATA_PATH;
		break;
	case 0:
		path = DATA_PATH
			"main.5826.com.beamdog.baldursgateIIenhancededition.obb";
		break;
	case 1:
		path = DATA_PATH
			"patch.5826.com.beamdog.baldursgateIIenhancededition.obb";
		break;
	default:
		path = "";
		break;
	}

	bg2v_log_printf(
		"[BG2V][JNI] getAPKPath kind=%d path=%s\n", path_kind, path);
	return jni->NewStringUTF(&jni, path);
}

static jobject getExternalStorageState(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	bg2v_log_printf("[BG2V][JNI] getExternalStorageState -> mounted\n");
	return jni->NewStringUTF(&jni, "mounted");
}

static jobject getFilesDir(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	/* File object token consumed only by getAbsolutePath below. */
	return (jobject)0x42420003;
}

static jobject getAbsolutePath(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	bg2v_log_printf("[BG2V][JNI] getAbsolutePath -> %s\n", DATA_PATH);
	return jni->NewStringUTF(&jni, DATA_PATH);
}

static jobject getExternalFilesDir(jmethodID id, va_list args) {
	(void)id;
	(void)va_arg(args, jobject);
	/* File object token consumed by getAbsolutePath. */
	return (jobject)0x42420004;
}

static jboolean isWiFiOn(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	/* Keep the bootstrap deterministic and offline. */
	return JNI_FALSE;
}

static jboolean writeToLog(jmethodID id, va_list args) {
	(void)id;
	jstring message = va_arg(args, jstring);
	if (message) {
		char *text = (char *)jni->GetStringUTFChars(&jni, message, NULL);
		if (text) {
			bg2v_log_printf("[BG2V][JAVA] %s\n", text);
			jni->ReleaseStringUTFChars(&jni, message, text);
		}
	}
	return JNI_TRUE;
}

static jobject getLanguageString(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	bg2v_log_printf("[BG2V][JNI] getLanguageString -> en_US\n");
	return jni->NewStringUTF(&jni, "en_US");
}

NameToMethodID nameToMethodId[] = {
	{ METHOD_GET_NATIVE_SURFACE, "getNativeSurface", METHOD_TYPE_OBJECT },
	{ METHOD_AUDIO_INIT, "audioInit", METHOD_TYPE_INT },
	{ METHOD_AUDIO_WRITE_SHORT_BUFFER, "audioWriteShortBuffer", METHOD_TYPE_VOID },
	{ METHOD_AUDIO_WRITE_BYTE_BUFFER, "audioWriteByteBuffer", METHOD_TYPE_VOID },
	{ METHOD_AUDIO_QUIT, "audioQuit", METHOD_TYPE_VOID },
	{ METHOD_POLL_INPUT_DEVICES, "pollInputDevices", METHOD_TYPE_VOID },
	{ METHOD_INPUT_GET_INPUT_DEVICE_IDS, "inputGetInputDeviceIds", METHOD_TYPE_OBJECT },
	{ METHOD_SEND_MESSAGE, "sendMessage", METHOD_TYPE_BOOLEAN },
	{ METHOD_GET_CONTEXT, "getContext", METHOD_TYPE_OBJECT },
	{ METHOD_GET_APK_PATH, "getAPKPath", METHOD_TYPE_OBJECT },
	{ METHOD_GET_EXTERNAL_STORAGE_STATE, "getExternalStorageState", METHOD_TYPE_OBJECT },
	{ METHOD_GET_FILES_DIR, "getFilesDir", METHOD_TYPE_OBJECT },
	{ METHOD_GET_ABSOLUTE_PATH, "getAbsolutePath", METHOD_TYPE_OBJECT },
	{ METHOD_GET_EXTERNAL_FILES_DIR, "getExternalFilesDir", METHOD_TYPE_OBJECT },
	{ METHOD_IS_WIFI_ON, "IsWiFiOn", METHOD_TYPE_BOOLEAN },
	{ METHOD_WRITE_TO_LOG, "writeToLog", METHOD_TYPE_BOOLEAN },
	{ METHOD_GET_LANGUAGE_STRING, "getLanguageString", METHOD_TYPE_OBJECT },
	{ METHOD_SHOW_TEXT_INPUT, "showTextInput", METHOD_TYPE_BOOLEAN },
};

MethodsBoolean methodsBoolean[] = {
	{ METHOD_SEND_MESSAGE, sendMessage },
	{ METHOD_IS_WIFI_ON, isWiFiOn },
	{ METHOD_WRITE_TO_LOG, writeToLog },
	{ METHOD_SHOW_TEXT_INPUT, showTextInput },
};
MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};
MethodsInt methodsInt[] = {
	{ METHOD_AUDIO_INIT, audioInit },
};
MethodsLong methodsLong[] = {};
MethodsObject methodsObject[] = {
	{ METHOD_GET_NATIVE_SURFACE, getNativeSurface },
	{ METHOD_INPUT_GET_INPUT_DEVICE_IDS, inputGetInputDeviceIds },
	{ METHOD_GET_CONTEXT, getContext },
	{ METHOD_GET_APK_PATH, getAPKPath },
	{ METHOD_GET_EXTERNAL_STORAGE_STATE, getExternalStorageState },
	{ METHOD_GET_FILES_DIR, getFilesDir },
	{ METHOD_GET_ABSOLUTE_PATH, getAbsolutePath },
	{ METHOD_GET_EXTERNAL_FILES_DIR, getExternalFilesDir },
	{ METHOD_GET_LANGUAGE_STRING, getLanguageString },
};
MethodsShort methodsShort[] = {};
MethodsVoid methodsVoid[] = {
	{ METHOD_AUDIO_WRITE_SHORT_BUFFER, audioWriteShortBuffer },
	{ METHOD_AUDIO_WRITE_BYTE_BUFFER, audioWriteByteBuffer },
	{ METHOD_AUDIO_QUIT, audioQuit },
	{ METHOD_POLL_INPUT_DEVICES, pollInputDevices },
};

/*
 * JNI Fields
*/

// System-wide constant that applications sometimes request
// https://developer.android.com/reference/android/content/Context.html#WINDOW_SERVICE
char WINDOW_SERVICE[] = "window";

// System-wide constant that's often used to determine Android version
// https://developer.android.com/reference/android/os/Build.VERSION.html#SDK_INT
// Possible values: https://developer.android.com/reference/android/os/Build.VERSION_CODES
const int SDK_INT = 19; // Android 4.4 / KitKat

NameToFieldID nameToFieldId[] = {
	{ 0, "WINDOW_SERVICE", FIELD_TYPE_OBJECT },
	{ 1, "SDK_INT", FIELD_TYPE_INT },
	{ 2, "mSeparateMouseAndTouch", FIELD_TYPE_BOOLEAN },
};

FieldsBoolean fieldsBoolean[] = {
	{ 2, JNI_FALSE },
};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {
		{ 1, SDK_INT },
};
FieldsObject fieldsObject[] = {
		{ 0, WINDOW_SERVICE },
};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES
