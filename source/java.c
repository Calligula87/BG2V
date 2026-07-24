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
};

static jobject getNativeSurface(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	/* Stable non-null token; VitaGL owns the real display surface. */
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

NameToMethodID nameToMethodId[] = {
	{ METHOD_GET_NATIVE_SURFACE, "getNativeSurface", METHOD_TYPE_OBJECT },
	{ METHOD_AUDIO_INIT, "audioInit", METHOD_TYPE_INT },
	{ METHOD_AUDIO_WRITE_SHORT_BUFFER, "audioWriteShortBuffer", METHOD_TYPE_VOID },
	{ METHOD_AUDIO_WRITE_BYTE_BUFFER, "audioWriteByteBuffer", METHOD_TYPE_VOID },
	{ METHOD_AUDIO_QUIT, "audioQuit", METHOD_TYPE_VOID },
	{ METHOD_POLL_INPUT_DEVICES, "pollInputDevices", METHOD_TYPE_VOID },
};

MethodsBoolean methodsBoolean[] = {};
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
};

FieldsBoolean fieldsBoolean[] = {};
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
