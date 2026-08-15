#include "AndroidAssetPackage.h"
#include "core/BinaryData.h"
#include "components/Exceptions.h"
#include "utils/AssetUtils.h"
#include "utils/FileUtils.h"
#include "utils/Log.h"

#include <algorithm>

namespace massif {

    AndroidAssetPackage::AndroidAssetPackage(const std::string& basePath) :
        _basePath(FileUtils::NormalizePath(basePath)),
        _baseAssetPackage(),
        _localAssetNames(),
        _localAssetNamesValid(false)
    {
        if (getLocalAssetNames().empty()) {
            throw FileException("No assets found in the asset directory", basePath);
        }
    }

    AndroidAssetPackage::AndroidAssetPackage(const std::string& basePath, const std::shared_ptr<AssetPackage>& baseAssetPackage) :
        _basePath(FileUtils::NormalizePath(basePath)),
        _baseAssetPackage(baseAssetPackage),
        _localAssetNames(),
        _localAssetNamesValid(false)
    {
        if (getLocalAssetNames().empty()) {
            throw FileException("No assets found in the asset directory", basePath);
        }
    }

    AndroidAssetPackage::~AndroidAssetPackage() {
    }

    std::string AndroidAssetPackage::getBasePath() const {
        return _basePath;
    }

    std::vector<std::string> AndroidAssetPackage::getLocalAssetNames() const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (!_localAssetNamesValid) {
            _localAssetNames.clear();
            ScanAssets(_basePath, std::string(), _localAssetNames);
            _localAssetNamesValid = true;
        }
        return _localAssetNames;
    }

    void AndroidAssetPackage::reload() {
        std::lock_guard<std::mutex> lock(_mutex);

        _localAssetNames.clear();
        _localAssetNamesValid = false;
    }

    std::vector<std::string> AndroidAssetPackage::getAssetNames() const {
        std::vector<std::string> names;
        if (_baseAssetPackage) {
            names = _baseAssetPackage->getAssetNames();
        }

        std::vector<std::string> localNames = getLocalAssetNames();
        names.reserve(names.size() + localNames.size());
        for (auto it = localNames.begin(); it != localNames.end(); it++) {
            if (std::find(names.begin(), names.end(), *it) == names.end()) {
                names.push_back(*it);
            }
        }
        return names;
    }

    std::shared_ptr<BinaryData> AndroidAssetPackage::loadAsset(const std::string& name) const {
        std::string normalizedName = FileUtils::NormalizePath(name);
        // Do not allow escaping the asset directory
        if (normalizedName.empty() || normalizedName.front() == '/' || normalizedName.compare(0, 2, "..") == 0) {
            if (_baseAssetPackage) {
                return _baseAssetPackage->loadAsset(name);
            }
            return std::shared_ptr<BinaryData>();
        }

        std::string path = _basePath.empty() ? normalizedName : _basePath + "/" + normalizedName;
        if (!AssetUtils::AssetExists(path)) {
            if (_baseAssetPackage) {
                return _baseAssetPackage->loadAsset(name);
            }
            return std::shared_ptr<BinaryData>();
        }
        return AssetUtils::LoadAsset(path);
    }

    void AndroidAssetPackage::ScanAssets(const std::string& basePath, const std::string& subDir, std::vector<std::string>& assetNames) {
        std::string fullPath = subDir.empty() ? basePath : (basePath.empty() ? subDir : basePath + "/" + subDir);

        // AssetManager.list does not say whether an entry is a file or a directory: an entry that
        // lists something is a directory, an entry that opens as an asset is a file. An empty
        // directory answers neither and is simply skipped.
        for (const std::string& name : AssetUtils::ListAssets(fullPath)) {
            if (name.empty()) {
                continue;
            }
            std::string relPath = subDir.empty() ? name : subDir + "/" + name;
            std::string entryPath = fullPath.empty() ? name : fullPath + "/" + name;
            if (AssetUtils::AssetExists(entryPath)) {
                assetNames.push_back(relPath);
            } else {
                ScanAssets(basePath, relPath, assetNames);
            }
        }
    }

}
