# Jtik
## 简介
Jtik是一个在android平台，以Java方法为粒度的运行时动态hook框架。其实现基于 [ART TI](https://source.android.google.cn/docs/core/runtime/art-ti)，可以hook应用本身以及系统java方法。
由于使用的是系统公开接口，不涉及ART虚拟机内部的ArtMethod等内存结构修改，理论上适配性会比较强。
## 使用
### 1. 添加依赖
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
### 2. 初始化
```java
//if you want hook framework method,like Activity.onCreate
//JtikConfig.needHookSystemClass = true; 

Jtik.init(context);
```

### 3. hook目标方法：
举例：有一个类`Test`
```java
public class Test {
   public int run(int a， int b){
	...
   }
}
```
Hook 类`Test`的`run`方法：
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
## 支持情况
理论上Android 8.0+全支持
## 参考
1. [ART TI](https://source.android.google.cn/docs/core/runtime/art-ti)
2. [slicer](https://cs.android.com/android/platform/superproject/main/+/main:tools/dexter/slicer/)
3. [android studio](https://cs.android.com/android-studio/platform/tools/base/+/mirror-goog-studio-main:deploy/agent/native/transform/)
4. [JVMTI Tool Interface](https://docs.oracle.com/javase/7/docs/platform/jvmti/jvmti.html#SpecificationIntro)
