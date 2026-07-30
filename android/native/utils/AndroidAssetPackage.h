/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_ANDROIDASSETPACKAGE_H_
#define _CARTO_ANDROIDASSETPACKAGE_H_

#include "utils/AssetPackage.h"

#include <mutex>

namespace carto {

    /**
     * An asset package based on the application-bundled Android assets (the 'assets' folder of
     * the APK). This is the counterpart of DirAssetPackage for assets that ship with the app:
     * APK assets are entries inside the package, not files, so they cannot be read through a
     * file system path.
     *
     * Note: the asset manager must be connected before an instance is created, which happens
     * when a MapView is constructed.
     */
    class AndroidAssetPackage : public AssetPackage {
    public:
        /**
         * Constructs an asset package from the given directory inside the application assets.
         * @param basePath The asset directory, relative to the assets root ("" is the root itself).
         * @throws std::exception If the directory contains no assets.
         */
        explicit AndroidAssetPackage(const std::string& basePath);
        /**
         * Constructs an asset package from the given directory inside the application assets,
         * with a fallback asset package.
         * @param basePath The asset directory, relative to the assets root ("" is the root itself).
         * @param baseAssetPackage The base asset package. If an asset is not found in the assets,
         *        the base asset package is used.
         * @throws std::exception If the directory contains no assets.
         */
        AndroidAssetPackage(const std::string& basePath, const std::shared_ptr<AssetPackage>& baseAssetPackage);
        virtual ~AndroidAssetPackage();

        /**
         * Returns the asset directory this package reads from.
         * @return The asset directory, relative to the assets root.
         */
        std::string getBasePath() const;

        /**
         * Returns the list of assets bundled under the base path, ignoring the base asset package.
         * @return The list of asset names, relative to the base path.
         */
        std::vector<std::string> getLocalAssetNames() const;

        /**
         * Rescans the asset directory. Bundled assets cannot change while the app runs, so this is
         * only needed if the asset manager was connected after this package was created.
         */
        void reload();

        virtual std::vector<std::string> getAssetNames() const;

        virtual std::shared_ptr<BinaryData> loadAsset(const std::string& name) const;

    private:
        static void ScanAssets(const std::string& basePath, const std::string& subDir, std::vector<std::string>& assetNames);

        const std::string _basePath;
        const std::shared_ptr<AssetPackage> _baseAssetPackage;

        mutable std::vector<std::string> _localAssetNames;
        mutable bool _localAssetNamesValid;

        mutable std::mutex _mutex;
    };

}

#endif
