#include <jni.h>
#include "tinywindow.h"

/* ── Helpers ─────────────────────────────────────────────────────── */

static const char *get_string(JNIEnv *env, jstring jstr) {
    if (!jstr) return NULL;
    return (*env)->GetStringUTFChars(env, jstr, NULL);
}

static void release_string(JNIEnv *env, jstring jstr, const char *cstr) {
    if (cstr) (*env)->ReleaseStringUTFChars(env, jstr, cstr);
}

/* ── Timing CMS JNI ─────────────────────────────────────────────── */

JNIEXPORT jlong JNICALL
Java_io_tinywindow_Native_tcmsCreate(JNIEnv *env, jclass cls,
                                      jint depth, jint width,
                                      jint numSlots, jlong slotDurationMs) {
    (void)env; (void)cls;
    tw_timing_cms_t *cms = tw_tcms_create(
        (uint32_t)depth, (uint32_t)width,
        (uint32_t)numSlots, (uint64_t)slotDurationMs
    );
    return (jlong)(uintptr_t)cms;
}

JNIEXPORT void JNICALL
Java_io_tinywindow_Native_tcmsDestroy(JNIEnv *env, jclass cls, jlong handle) {
    (void)env; (void)cls;
    tw_tcms_destroy((tw_timing_cms_t *)(uintptr_t)handle);
}

JNIEXPORT jint JNICALL
Java_io_tinywindow_Native_tcmsRecord(JNIEnv *env, jclass cls,
                                      jlong handle, jstring key, jlong nowMs) {
    (void)cls;
    const char *ckey = get_string(env, key);
    jint result = (jint)tw_tcms_record(
        (tw_timing_cms_t *)(uintptr_t)handle, ckey, (uint64_t)nowMs
    );
    release_string(env, key, ckey);
    return result;
}

JNIEXPORT jlong JNICALL
Java_io_tinywindow_Native_tcmsCount(JNIEnv *env, jclass cls,
                                     jlong handle, jstring key,
                                     jlong windowMs, jlong nowMs) {
    (void)cls;
    const char *ckey = get_string(env, key);
    jlong result = (jlong)tw_tcms_count(
        (tw_timing_cms_t *)(uintptr_t)handle, ckey,
        (uint64_t)windowMs, (uint64_t)nowMs
    );
    release_string(env, key, ckey);
    return result;
}

JNIEXPORT void JNICALL
Java_io_tinywindow_Native_tcmsReset(JNIEnv *env, jclass cls, jlong handle) {
    (void)env; (void)cls;
    tw_tcms_reset((tw_timing_cms_t *)(uintptr_t)handle);
}

JNIEXPORT jlong JNICALL
Java_io_tinywindow_Native_tcmsMemoryUsage(JNIEnv *env, jclass cls, jlong handle) {
    (void)env; (void)cls;
    return (jlong)tw_tcms_memory_usage((const tw_timing_cms_t *)(uintptr_t)handle);
}

/* ── Sliding Bloom Filter JNI ───────────────────────────────────── */

JNIEXPORT jlong JNICALL
Java_io_tinywindow_Native_sbfCreate(JNIEnv *env, jclass cls,
                                     jlong expectedItems, jdouble fpRate,
                                     jint numSlots, jlong slotDurationMs) {
    (void)env; (void)cls;
    tw_sliding_bf_t *bf = tw_sbf_create(
        (uint64_t)expectedItems, (double)fpRate,
        (uint32_t)numSlots, (uint64_t)slotDurationMs
    );
    return (jlong)(uintptr_t)bf;
}

JNIEXPORT void JNICALL
Java_io_tinywindow_Native_sbfDestroy(JNIEnv *env, jclass cls, jlong handle) {
    (void)env; (void)cls;
    tw_sbf_destroy((tw_sliding_bf_t *)(uintptr_t)handle);
}

JNIEXPORT jint JNICALL
Java_io_tinywindow_Native_sbfInsert(JNIEnv *env, jclass cls,
                                     jlong handle, jstring key, jlong nowMs) {
    (void)cls;
    const char *ckey = get_string(env, key);
    jint result = (jint)tw_sbf_insert(
        (tw_sliding_bf_t *)(uintptr_t)handle, ckey, (uint64_t)nowMs
    );
    release_string(env, key, ckey);
    return result;
}

JNIEXPORT jboolean JNICALL
Java_io_tinywindow_Native_sbfMightContain(JNIEnv *env, jclass cls,
                                           jlong handle, jstring key,
                                           jlong windowMs, jlong nowMs) {
    (void)cls;
    const char *ckey = get_string(env, key);
    jboolean result = (jboolean)tw_sbf_might_contain(
        (tw_sliding_bf_t *)(uintptr_t)handle, ckey,
        (uint64_t)windowMs, (uint64_t)nowMs
    );
    release_string(env, key, ckey);
    return result;
}

JNIEXPORT jlong JNICALL
Java_io_tinywindow_Native_sbfMemoryUsage(JNIEnv *env, jclass cls, jlong handle) {
    (void)env; (void)cls;
    return (jlong)tw_sbf_memory_usage((const tw_sliding_bf_t *)(uintptr_t)handle);
}
