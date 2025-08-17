#include <jni.h>
#include <string>
#include "jvmti.h"
#include <sstream>
#include <dlfcn.h>
#include "log.h"
#include "jni/jni_util.h"
#include "transform/hook_transform.h"
#include "hooker/hook_bridge.h"
#include "jni/dl_util.h"
#include "agent/jtik_agent.h"


static void *sAgentHandle = nullptr;
static std::string sAgentPath;

enum jdwpState{
    UNKNOWN,
    ON,
    OFF
};

static jdwpState mOrgJdwpState = UNKNOWN;

template <typename FuncType>
static FuncType getAgentFn(const char * funcName) {
    if (sAgentHandle == nullptr && !sAgentPath.empty()) {
        sAgentHandle = DlUtil::dlopen(sAgentPath.c_str(),RTLD_NOW, true);
    }
    CHECK_NULL_RETURN_NULL(sAgentHandle,"hook AgentHandle null")
    return reinterpret_cast<FuncType>(::dlsym(sAgentHandle, funcName));
}

static bool SetDebuggableRelease(bool allowDebug) {
    ALOGI("try setDebuggableRelease %d", allowDebug);
    void *handle = DlUtil::dlopen("libart.so", RTLD_NOW);
    CHECK_NULL_RETURN_BOOL(handle,"open libart fail");
    DlHandler dlHandler = handle;

    if (mOrgJdwpState == UNKNOWN) {
        auto (*IsJdwpAllowed)() = reinterpret_cast<bool (*)()>(
                ::dlsym(dlHandler.GetHandle(), "_ZN3art3Dbg13IsJdwpAllowedEv"));
        bool sJdwpAllowed = IsJdwpAllowed();
        ALOGI("orgJdwpAllowed %d", sJdwpAllowed);
        mOrgJdwpState = sJdwpAllowed ? jdwpState::ON : jdwpState::OFF;
    }

    if (mOrgJdwpState == jdwpState::OFF) {
        auto (*SetJdwpAllowedFn)(bool) = reinterpret_cast<void (*)(bool)>(
                ::dlsym(dlHandler.GetHandle(), "_ZN3art3Dbg14SetJdwpAllowedEb"));
        CHECK_NULL_RETURN_BOOL(SetJdwpAllowedFn,"SetJdwpAllowedFn get fail");
        SetJdwpAllowedFn(allowDebug);
    }

//    auto (*setJavaDebuggableFn)(void *, bool) = reinterpret_cast<void (*)(void *, bool)>(
//            ::dlsym(dlHandler.GetHandle(), "_ZN3art7Runtime17SetJavaDebuggableEb"));
//    CHECK_NULL_RETURN_BOOL(setJavaDebuggableFn,"setJavaDebuggableFn get fail");
//    void **runtimeInstance_ = static_cast<void **>(
//            ::dlsym(dlHandler.GetHandle(), "_ZN3art7Runtime9instance_E"));
//    CHECK_NULL_RETURN_BOOL(runtimeInstance_,"runtime instance get fail");
//    setJavaDebuggableFn(*runtimeInstance_, allowDebug);
    return true;
}

static std::string
ClassNameToDescriptor(const char *class_name) {
    std::stringstream ss;
    ss << "L";
    for (auto p = class_name; *p != '\0'; ++p) {
        ss << (*p == '.' ? '/' : *p);
    }
    ss << ";";
    return ss.str();
}

extern "C"
JNIEXPORT jboolean JNICALL
JtikInitAfterAgent(JNIEnv *env, jclass clazz, jstring bridge_class,
                   jstring enter_method_name,
                   jstring exit_method_name, jstring static_exit_method_name,
                   jstring modify_param_method_name, jstring static_modify_param_method_name,
                   jstring agent_path,
                   jstring pathAppendSys,
                   jboolean setNoDebuggable) {
    jni::j2c_Utf8String(env, agent_path, sAgentPath);
    if (sAgentPath.empty()) {
        ALOGE("agent string failed in init");
        return false;
    }
    sAgentHandle = DlUtil::dlopen(sAgentPath.c_str(),RTLD_NOW, true);
    if (sAgentHandle == nullptr) {
        ALOGW("find loaded agent failed");
        sAgentHandle = ::dlopen(sAgentPath.c_str(),RTLD_NOW);
        CHECK_NULL_RETURN_BOOL(sAgentHandle, "dlopen agent failed")
    }
    std::string bridgeClassName, bridgeMethodEntry, bridgeMethodExit, bridgeStaticMethodExit, bridgeMethodModifyParam, bridgeStaticMethodModifyParam;
    bool success = jni::j2c_Utf8String(env, bridge_class, bridgeClassName)
            && jni::j2c_Utf8String(env, enter_method_name, bridgeMethodEntry)
            && jni::j2c_Utf8String(env, exit_method_name, bridgeMethodExit)
            && jni::j2c_Utf8String(env, static_exit_method_name, bridgeStaticMethodExit)
            && jni::j2c_Utf8String(env, modify_param_method_name, bridgeMethodModifyParam)
            && jni::j2c_Utf8String(env, static_modify_param_method_name, bridgeStaticMethodModifyParam);
    if(success
            && !bridgeClassName.empty()
            && !bridgeMethodEntry.empty()
            && !bridgeMethodExit.empty()
            && !bridgeStaticMethodExit.empty()
            && !bridgeMethodModifyParam.empty()
            && !bridgeStaticMethodModifyParam.empty()) {
        std::string nativeName = ClassNameToDescriptor(bridgeClassName.c_str());
        auto setBridgeInfoFn = getAgentFn<AgentSetBridgeInfoFn>(AgentSetBridgeInfoFnName.c_str());
        setBridgeInfoFn(nativeName.c_str(), bridgeMethodEntry.c_str(), bridgeMethodExit.c_str(), bridgeStaticMethodExit.c_str(),
                         bridgeMethodModifyParam.c_str(), bridgeStaticMethodModifyParam.c_str());

        jclass javaClassClz = env->FindClass("java/lang/Class");
        CHECK_NULL_RETURN_BOOL(javaClassClz, "java class not found")
        jmethodID getClassLoaderMethod = env->GetMethodID(javaClassClz, "getClassLoader", "()Ljava/lang/ClassLoader;");
        CHECK_NULL_RETURN_BOOL(getClassLoaderMethod, "getClassLoader method not found")

        jobject classLoader = env->CallObjectMethod(clazz, getClassLoaderMethod);
        CHECK_NULL_RETURN_BOOL(javaClassClz, "get hooker classloader fail")
        jobject gClassLoader= env->NewGlobalRef(classLoader);
        CHECK_NULL_RETURN_BOOL(gClassLoader, "new global cl fail")
        auto saveClassLoaderFn = getAgentFn<AgentSaveHookerClassloaderFn>( AgentSaveHookerClassloaderFnName.c_str());
        if (saveClassLoaderFn) {
            saveClassLoaderFn(gClassLoader);
        }

        std::string appendPath;
        if (pathAppendSys && jni::j2c_Utf8String(env, pathAppendSys, appendPath) && !appendPath.empty()) {
            auto addPathFunc = getAgentFn<AgentAddBootClassLoaderFn>(AgentAddBootClassLoaderFnName.c_str());
            if (addPathFunc != nullptr) {
                addPathFunc(appendPath.c_str());
                ALOGI("AddToBootstrapClassLoaderSearch:%s", appendPath.c_str());
            }
        }
        if (setNoDebuggable) {
            SetDebuggableRelease(false);
        }
        return true;
    } else {
        return false;
    }
}

extern "C" JNIEXPORT jlong JNICALL JtikDoHookTransform(JNIEnv *env,
                                                       jclass clazz,
                                                       jobject jTargetMethod,
                                                       jboolean jhookEnter,
                                                       jboolean jhookExit,
                                                       jintArray jparaIndexes) {
    ALOGI("jtikDoHookTransform native begin");
    auto transformFn = getAgentFn<AgentDoTransformFn>(AgentDoTransformFnName.c_str());
    if (transformFn == nullptr) {
        ALOGI("jtikDoHookTransform get agent func fail");
        return 0;
    }
    return transformFn(env, jTargetMethod, jhookEnter, jhookExit, jparaIndexes);
}

extern "C" JNIEXPORT jboolean JNICALL JtikDoUnHookTransform(JNIEnv *env,
                                                       jclass clazz,
                                                       jlong jTargetMethod) {
    ALOGI("JtikDoUnHookTransform native begin");
    auto transformFn = getAgentFn<AgentDoUnHookTransformFn>(AgentDoUnHookTransformFnName.c_str());
    CHECK_NULL_RETURN_BOOL(transformFn,"JtikDoUnHookTransform get agent function fail");
    return transformFn(env,jTargetMethod);
}

extern "C" JNIEXPORT jobject JNICALL JtikGetHookerClassLoader(JNIEnv *env,
                                                            jclass clazz, jstring agent) {
    ALOGI("JtikGetHookerClassLoader native begin");
    std::string cAgent;
    jni::j2c_Utf8String(env, agent, cAgent);
    if(cAgent.empty()) {
        return nullptr;
    }
    auto handle = DlUtil::dlopen(cAgent.c_str(),RTLD_NOW, true);
    if (handle == nullptr) {
        return nullptr;
    }
    auto getClassLoaderFn = reinterpret_cast<AgentGetHookerClassloaderFn>(::dlsym(handle, AgentGetHookerClassloaderFnName.c_str()));
    if (getClassLoaderFn) {
        return getClassLoaderFn();
    }
    return nullptr;
}

static JNINativeMethod methods[] = {
        {"initAfterAgentAttach", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)Z", reinterpret_cast<void *>(JtikInitAfterAgent)},
        {"doHookTransform", "(Ljava/lang/reflect/Member;ZZ[I)J", reinterpret_cast<void *>(JtikDoHookTransform)},
        {"doUnHookTransform", "(J)Z", reinterpret_cast<void *>(JtikDoUnHookTransform)},
        {"getHookerClassLoader", "(Ljava/lang/String;)Ljava/lang/Object;", reinterpret_cast<void *>(JtikGetHookerClassLoader)}
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    ALOGI("JNI_OnLoad");
    jclass clazz = env->FindClass("com/zxc/jtik/Jtik");
    env->RegisterNatives(clazz, methods, sizeof(methods)/sizeof(methods[0]));
    SetDebuggableRelease(true);
    return JNI_VERSION_1_6;
}