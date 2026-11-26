// LibertyRecomp Android — :app module.
//
// Gradle never runs CMake. The native side (libmain.so, librexruntime.so,
// the GPU plugins, libSDL3.so, libc++_shared.so) is millions of lines of
// recompiled PowerPC and is built by os/android/scripts/build_native.sh,
// which stages the libraries into src/main/jniLibs and the title resources
// into src/main/assets/Resources. This module only packages them.
//
// The SDL Java classes come from the same SDL3 tree the native library is
// built from: SDLActivity refuses to start when the versions differ.

plugins {
    id("com.android.application")
}

val sdlJavaDir = rootProject.file(
    "../../glue/rexglue-sdk-main/thirdparty/sdl3/android-project/app/src/main/java")

// GPU family this APK is built for: -PlibertyGpu=adreno (default) or =mali.
// "mali" makes the device's own Vulkan driver the default instead of the
// bundled Mesa Turnip (which only exists for Adreno), drops the Adreno-only
// TU_DEBUG setting, and tags the version name with "-mali". Everything else is
// identical, so an Adreno and a Mali APK differ only in first-launch defaults;
// the player can still switch drivers on the launcher either way.
val libertyGpu = (project.findProperty("libertyGpu") as String? ?: "adreno").lowercase()
require(libertyGpu == "adreno" || libertyGpu == "mali") {
    "-PlibertyGpu must be 'adreno' or 'mali', got '$libertyGpu'"
}
// Dedicated independently installable package, never overwriting or reading
// the classic app's sandbox. Reuse the verified Armv9 Mali native libraries.
val libertyEdition = (project.findProperty("libertyEdition") as String? ?: "classic").lowercase()
require(libertyEdition == "classic" || libertyEdition == "g925native" || libertyEdition == "g925x") {
    "-PlibertyEdition must be 'classic', 'g925native' or 'g925x'"
}
val g925NativeEdition = libertyEdition == "g925native"
// New Android identity for the device-specific renderer laboratory.
// It never modifies the installed Native v3.5 or classic app.
val g925XEdition = libertyEdition == "g925x"
require((!g925NativeEdition && !g925XEdition) || libertyGpu == "mali") {
    "G925 Native requires the system Mali Vulkan build"
}

android {
    namespace   = "com.libertyrecomp"
    compileSdk  = 35
    ndkVersion  = "29.0.14206865"

    // One fixed key for every build (Mali Edition alpha 1.4 and later), so a
    // new APK installs over the previous one as an update. GitHub's build
    // machines would otherwise sign each build with a new random debug key.
    signingConfigs {
        getByName("debug") {
            storeFile     = file("liberty-mali.keystore")
            storePassword = "android"
            keyAlias      = "liberty-mali"
            keyPassword   = "android"
        }
    }

    buildFeatures {
        buildConfig = true
    }

    defaultConfig {
        applicationId = if (g925XEdition) "com.zmodliberty.g925x" else if (g925NativeEdition) "com.zmodliberty.g925native" else "com.libertyrecomp"
        // Android launcher label is per package; the old installation keeps
        // its exact package name, signing key and config untouched.
        manifestPlaceholders["libertyAppLabel"] = if (g925XEdition) "ZMOD G925 X Lab" else if (g925NativeEdition)
            "ZMOD Liberty G925 Native" else "Liberty Recompiled"
        // 28: ASharedMemory (26) for guest memory and AAudio (27) for audio.
        minSdk        = 29
        targetSdk     = 35
        // Higher than every earlier build (upstream 0.5.9.3 and Mali alpha 1.4 are 18),
        // so it installs as an update over either.
        versionCode   = 72
        // Keep the original edition's numeric build metadata stable.
        if (g925XEdition) versionCode = 79
        // Shown as e.g. 0.5.9.3-mali-alpha-1.5. The workflow reads both the
        // base version and versionSuffix to name the APK.
        val versionSuffix = "-alpha-2.44-g925-rebased"
        versionName   = "0.5.9.3" + (if (libertyGpu == "mali") "-mali" else "") +
            versionSuffix + (if (g925NativeEdition) "-G925Native-v3.5" else "") +
            (if (g925XEdition) "-G925X-v0.9" else "")

        // Read by DriverBridge.defaultMode(): the driver used when driver.txt
        // does not exist yet.
        buildConfigField("String", "DEFAULT_DRIVER",
            if (libertyGpu == "mali") "\"system\"" else "\"turnip\"")
        buildConfigField("String", "GPU_FAMILY", "\"$libertyGpu\"")
        buildConfigField("boolean", "G925_NATIVE_EDITION", g925NativeEdition.toString())
        buildConfigField("boolean", "G925_X_EDITION", g925XEdition.toString())

        ndk {
            abiFilters += listOf("arm64-v8a")
        }
    }

    sourceSets["main"].java.srcDirs("src/main/java", sdlJavaDir)

    buildTypes {
        debug {
            isDebuggable    = true
            isJniDebuggable = true
            isMinifyEnabled = false
        }
        release {
            isMinifyEnabled = false
            signingConfig   = signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    packaging {
        jniLibs {
            // Unpack the libraries into nativeLibraryDir: the runtime finds its
            // GPU plugins by real path next to itself, and a Turnip driver
            // loaded through libadrenotools needs real files as well.
            useLegacyPackaging = true
            // build_native.sh decides what is stripped.
            keepDebugSymbols += "**/*.so"
        }
    }

    lint {
        checkReleaseBuilds = false
        abortOnError = false
    }
}
