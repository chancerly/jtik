# Jtik

[中文版本](README_cn.md)
## Introduction
Jtik is a runtime dynamic hook framework on the Android platform that operates at the granularity of Java methods.Its implementation is based on [ART TI](https://source.android.google.cn/docs/core/runtime/art-ti) and it is capable of hooking both the application itself and system Java methods. Since it utilizes publicly available system interfaces and does not involve modifications to internal memory structures of the ART virtual machine, such as ArtMethod, it theoretically has strong compatibility.
## Usage
### 1. Add dependency
[![](https://jitpack.io/v/chancerly/jtik.svg)](https://jitpack.io/#chancerly/jtik)

```gradle
repositories {
        maven { url 'https://jitpack.io' }
}
```

 ```gradle
dependencies {
    implementation 'com.github.chancerly:jtik:0.0.1'
}
```

### 2. Initialization
```java
//if you want hook framework method,like Activity.onCreate
//JtikConfig.needHookSystemClass = true; 

Jtik.init(context);
```

### 3. hook target method：
for example：as a class `Test`
```java
public class Test {
   public int run(int a， int b){
	...
   }
}
```
hook the method `run` of class `Test`：
```java
Jtik.hook(classLoader.loadClass("Test").getDeclaredMethod("run", int.class, int.class),
		new com.zxc.jtik.MethodHook.Builder().setMethodEnterListener((o, objects) -> {
            //do something on method enter...
        }).setMethodExitListener((o, o1) -> {
            //do something on method exit...
	        return o1;
        }).setParamModifier(0, (thisObject, inParam) -> {
            //do something if you want to modify the parameter...
	    return 6;//the parameter value you chage to
        }).build());
```
## Support platforms:
Theoretically, Android 8.0+ are fully supported
## Reference
1. [ART TI](https://source.android.google.cn/docs/core/runtime/art-ti)
2. [slicer](https://cs.android.com/android/platform/superproject/main/+/main:tools/dexter/slicer/)
3. [android studio](https://cs.android.com/android-studio/platform/tools/base/+/mirror-goog-studio-main:deploy/agent/native/transform/)
4. [JVMTI Tool Interface](https://docs.oracle.com/javase/7/docs/platform/jvmti/jvmti.html#SpecificationIntro)
