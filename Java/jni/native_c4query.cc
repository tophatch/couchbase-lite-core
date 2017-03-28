#include <c4Base.h>
#include "com_couchbase_litecore_C4Query.h"
#include "com_couchbase_litecore_C4QueryEnumerator.h"
#include "native_glue.hh"
#include "c4Query.h"
#include <errno.h>
#include <vector>
#include <c4.h>

using namespace litecore;
using namespace litecore::jni;

// ----------------------------------------------------------------------------
// com_couchbase_litecore_C4Query
// ----------------------------------------------------------------------------

/*
 * Class:     com_couchbase_litecore_C4Query
 * Method:    init
 * Signature: (JLjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_com_couchbase_litecore_C4Query_init(JNIEnv *env, jclass clazz, jlong db, jstring jexpr) {
    jstringSlice expr(env, jexpr);
    C4Error error = {};
    C4Query *query = c4query_new((C4Database *) db, expr, &error);
    if (!query)
        throwError(env, error);
    return (jlong) query;
}

/*
 * Class:     com_couchbase_litecore_C4Query
 * Method:    free
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_com_couchbase_litecore_C4Query_free(JNIEnv *env, jclass clazz, jlong jquery) {
    c4query_free((C4Query *) jquery);
}

/*
 * Class:     com_couchbase_litecore_C4Query
 * Method:    explain
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_couchbase_litecore_C4Query_explain(JNIEnv *env, jclass clazz, jlong jquery) {
    C4StringResult result = c4query_explain((C4Query *) jquery);
    jstring jstr = toJString(env, result);
    c4slice_free(result);
    return jstr;
}

/*
 * Class:     com_couchbase_litecore_C4Query
 * Method:    run
 * Signature: (JJJZLjava/lang/String;)J
 */
JNIEXPORT jlong JNICALL
Java_com_couchbase_litecore_C4Query_run(JNIEnv *env, jclass clazz,
                                        jlong jquery,
                                        jlong jskip, jlong jlimit, jboolean jrankFullText,
                                        jstring jencodedParameters) {
    C4QueryOptions options = {
            (uint64_t) std::max((long long) jskip, 0ll),
            (uint64_t) std::max((long long) jlimit, 0ll),
            (bool) jrankFullText
    };
    jstringSlice encodedParameters(env, jencodedParameters);
    C4Error error;
    C4QueryEnumerator *e = c4query_run((C4Query *) jquery, &options, encodedParameters, &error);
    if (!e)
        throwError(env, error);
    return (jlong) e;
}

/*
 * Class:     com_couchbase_litecore_C4Query
 * Method:    fullTextMatched
 * Signature: (JLjava/lang/String;J)[B
 */
JNIEXPORT jbyteArray JNICALL
Java_com_couchbase_litecore_C4Query_fullTextMatched(JNIEnv *env, jclass clazz, jlong jquery,
                                                    jstring jdocid, jlong jseq) {
    jstringSlice docID(env, jdocid);
    C4SliceResult s = c4query_fullTextMatched((C4Query *) jquery, docID,
                                              (C4SequenceNumber) jseq, nullptr);
    jbyteArray res = toJByteArray(env, s);
    c4slice_free(s);
    return res;
}

// ----------------------------------------------------------------------------
// com_couchbase_litecore_C4QueryEnumerator
// ----------------------------------------------------------------------------
/*
 * Class:     com_couchbase_litecore_C4QueryEnumerator
 * Method:    customColumns
 * Signature: (J)J
 */
JNIEXPORT jbyteArray JNICALL
Java_com_couchbase_litecore_C4QueryEnumerator_customColumns(JNIEnv *env, jclass clazz,
                                                            jlong handle) {
    C4SliceResult s = c4queryenum_customColumns((C4QueryEnumerator *) handle);
    jbyteArray res = toJByteArray(env, s);
    c4slice_free(s);
    return res;
}

/*
 * Class:     com_couchbase_litecore_C4QueryEnumerator
 * Method:    fullTextMatched
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jbyteArray JNICALL
Java_com_couchbase_litecore_C4QueryEnumerator_fullTextMatched(JNIEnv *env, jclass clazz,
                                                              jlong handle) {
    C4SliceResult s = c4queryenum_fullTextMatched((C4QueryEnumerator *) handle, nullptr);
    jbyteArray res = toJByteArray(env, s);
    c4slice_free(s);
    return res;
}

/*
 * Class:     com_couchbase_litecore_C4QueryEnumerator
 * Method:    next
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_couchbase_litecore_C4QueryEnumerator_next(JNIEnv *env, jclass clazz, jlong handle) {
    if (!handle)
        return false;
    C4Error error;
    jboolean result = c4queryenum_next((C4QueryEnumerator *) handle, &error);
    if (!result) {
        // At end of iteration, proactively free the enumerator:
        c4queryenum_free((C4QueryEnumerator *) handle);
        if (error.code != 0)
            throwError(env, error);
    }
    return result;
}

/*
 * Class:     com_couchbase_litecore_C4QueryEnumerator
 * Method:    close
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_com_couchbase_litecore_C4QueryEnumerator_close(JNIEnv *env, jclass clazz, jlong handle) {
    if (!handle)
        return;
    c4queryenum_close((C4QueryEnumerator *) handle);
}

/*
 * Class:     com_couchbase_litecore_C4QueryEnumerator
 * Method:    free
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_com_couchbase_litecore_C4QueryEnumerator_free(JNIEnv *env, jclass clazz, jlong handle) {
    if (!handle)
        return;
    c4queryenum_free((C4QueryEnumerator *) handle);
}

/*
 * Class:     com_couchbase_litecore_C4QueryEnumerator
 * Method:    getDocID
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_couchbase_litecore_C4QueryEnumerator_getDocID(JNIEnv *env, jclass clazz, jlong handle) {
    if (!handle)
        return nullptr;
    return toJString(env, ((C4QueryEnumerator *) handle)->docID);
}

/*
 * Class:     com_couchbase_litecore_C4QueryEnumerator
 * Method:    getDocSequence
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL
Java_com_couchbase_litecore_C4QueryEnumerator_getDocSequence(JNIEnv *env, jclass clazz,
                                                             jlong handle) {
    if (!handle)
        return 0;
    return ((C4QueryEnumerator *) handle)->docSequence;
}

/*
 * Class:     com_couchbase_litecore_C4QueryEnumerator
 * Method:    getRevID
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_couchbase_litecore_C4QueryEnumerator_getRevID(JNIEnv *env, jclass clazz, jlong handle) {
    if (!handle)
        return nullptr;
    return toJString(env, ((C4QueryEnumerator *) handle)->revID);
}

/*
 * Class:     com_couchbase_litecore_C4QueryEnumerator
 * Method:    getDocFlags
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL
Java_com_couchbase_litecore_C4QueryEnumerator_getDocFlags(JNIEnv *env, jclass clazz, jlong handle) {
    if (!handle)
        return 0;
    return ((C4QueryEnumerator *) handle)->docFlags;
}
