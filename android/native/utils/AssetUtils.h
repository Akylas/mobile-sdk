/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_ASSETUTILS_H_
#define _CARTO_ASSETUTILS_H_

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <jni.h>
#include <android/asset_manager.h>

namespace carto {
    class BinaryData;

    /**
     * A helper class for managing application-bundled assets.
     */
    class AssetUtils {
    public:
        /**
         * Internal method for connecting to asset manager.
         * @param androidAssetManager The asset manager instance to use.
         */
        static void SetAssetManagerPointer(jobject androidAssetManager);

        /**
         * Loads the specified bundled asset.
         * @param path The path of the asset to load. The path is relative to application root folder.
         * @return The loaded asset as a byte vector or null if the asset was not found or could not be loaded.
         */
        static std::shared_ptr<BinaryData> LoadAsset(const std::string& path);

        /**
         * Returns true if the specified bundled asset exists and is a FILE (not a directory).
         * Unlike loadAsset this does not read or log anything, so it can be used to probe.
         * @param path The path of the asset, relative to the application root folder.
         * @return True if the asset exists as a file.
         */
        static bool AssetExists(const std::string& path);

        /**
         * Lists the names of the assets directly inside the specified bundled directory. The
         * listing is NOT recursive and does not tell files and directories apart - an entry is a
         * directory if listing it returns something, a file if assetExists returns true for it.
         * Note: the NDK asset API cannot enumerate directories at all, so this goes through the
         * Java AssetManager; the asset manager must already be connected (a MapView does that).
         * @param path The directory path, relative to the application root folder ("" is the root).
         * @return The names of the entries in that directory, without any path prefix.
         */
        static std::vector<std::string> ListAssets(const std::string& path);

    private:
        AssetUtils();

        static AAssetManager* _AssetManagerPtr;
        static jobject _AssetManagerRef;
        static std::mutex _Mutex;
    };

}

#endif
