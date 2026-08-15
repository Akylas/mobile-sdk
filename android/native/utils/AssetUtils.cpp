#include "AssetUtils.h"
#include "core/BinaryData.h"
#include "utils/AndroidUtils.h"
#include "utils/JNILocalFrame.h"
#include "utils/Log.h"

#include <android/asset_manager_jni.h>

namespace massif {

    void AssetUtils::SetAssetManagerPointer(jobject androidAssetManager) {
        std::lock_guard<std::mutex> lock(_Mutex);
        JNIEnv* jenv = AndroidUtils::GetCurrentThreadJNIEnv();
        _AssetManagerPtr = AAssetManager_fromJava(jenv, androidAssetManager);
        // The Java object is kept as well: the NDK asset API can load an asset but cannot list a
        // directory, and AssetManager.list can (see ListAssets).
        if (_AssetManagerRef) {
            jenv->DeleteGlobalRef(_AssetManagerRef);
            _AssetManagerRef = NULL;
        }
        if (androidAssetManager) {
            _AssetManagerRef = jenv->NewGlobalRef(androidAssetManager);
        }
    }

    std::shared_ptr<BinaryData> AssetUtils::LoadAsset(const std::string& path) {
        std::shared_ptr<AAsset> asset;
        {
            std::lock_guard<std::mutex> lock(_Mutex);
            if (!_AssetManagerPtr) {
                Log::Error("AssetManager::LoadAsset: Asset manager pointer not set yet");
                return std::shared_ptr<BinaryData>();
            }

            AAsset* assetRaw = AAssetManager_open(_AssetManagerPtr, path.c_str(), AASSET_MODE_UNKNOWN);
            if (!assetRaw) {
                Log::Errorf("AssetManager::LoadAsset: Asset not found: %s", path.c_str());
                return std::shared_ptr<BinaryData>();
            }
            asset = std::shared_ptr<AAsset>(assetRaw, AAsset_close);
        }

        int size = AAsset_getLength(asset.get());
        if (size < 0) {
            Log::Errorf("AssetManager::LoadAsset: Asset size is <0: %s", path.c_str());
            return std::shared_ptr<BinaryData>();
        }
        std::vector<unsigned char> data(size);

        AAsset_read(asset.get(), data.data(), size);

        return std::make_shared<BinaryData>(std::move(data));
    }

    bool AssetUtils::AssetExists(const std::string& path) {
        std::lock_guard<std::mutex> lock(_Mutex);
        if (!_AssetManagerPtr) {
            return false;
        }
        AAsset* assetRaw = AAssetManager_open(_AssetManagerPtr, path.c_str(), AASSET_MODE_UNKNOWN);
        if (!assetRaw) {
            return false; // missing, or a directory - opening a directory always fails
        }
        AAsset_close(assetRaw);
        return true;
    }

    std::vector<std::string> AssetUtils::ListAssets(const std::string& path) {
        std::vector<std::string> names;

        jobject assetManagerRef = NULL;
        {
            std::lock_guard<std::mutex> lock(_Mutex);
            assetManagerRef = _AssetManagerRef;
        }
        if (!assetManagerRef) {
            Log::Error("AssetUtils::ListAssets: Asset manager not set yet");
            return names;
        }

        JNIEnv* jenv = AndroidUtils::GetCurrentThreadJNIEnv();
        JNILocalFrame jframe(jenv, 32, "AssetUtils::ListAssets");
        if (!jframe.isValid()) {
            return names;
        }

        jclass assetManagerClass = jenv->GetObjectClass(assetManagerRef);
        jmethodID listMethod = jenv->GetMethodID(assetManagerClass, "list", "(Ljava/lang/String;)[Ljava/lang/String;");
        if (!listMethod) {
            jenv->ExceptionClear();
            Log::Error("AssetUtils::ListAssets: AssetManager.list not found");
            return names;
        }

        jstring jpath = jenv->NewStringUTF(path.c_str());
        jobjectArray jnames = static_cast<jobjectArray>(jenv->CallObjectMethod(assetManagerRef, listMethod, jpath));
        if (jenv->ExceptionCheck()) {
            // A non-existing path throws IOException; that is a normal 'nothing here' answer.
            jenv->ExceptionClear();
            return names;
        }
        if (!jnames) {
            return names;
        }

        jsize count = jenv->GetArrayLength(jnames);
        for (jsize i = 0; i < count; i++) {
            jstring jname = static_cast<jstring>(jenv->GetObjectArrayElement(jnames, i));
            if (!jname) {
                continue;
            }
            const char* nameRaw = jenv->GetStringUTFChars(jname, NULL);
            if (nameRaw) {
                names.push_back(nameRaw);
                jenv->ReleaseStringUTFChars(jname, nameRaw);
            }
            jenv->DeleteLocalRef(jname);
        }
        return names;
    }

    AssetUtils::AssetUtils() {
    }

    AAssetManager* AssetUtils::_AssetManagerPtr = nullptr;
    jobject AssetUtils::_AssetManagerRef = NULL;
    std::mutex AssetUtils::_Mutex;

}
