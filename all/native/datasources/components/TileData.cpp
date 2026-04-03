#include "TileData.h"
#include "core/BinaryData.h"
#include "core/Variant.h"

namespace carto {
    
    TileData::TileData(const std::shared_ptr<BinaryData>& data) :
        _data(data), _expirationTime(), _replaceWithParent(false), _overzoom(false), _metadata(), _mutex()
    {
    }

    TileData::~TileData() {
    }

    long long TileData::getMaxAge() const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (!_expirationTime) {
            return -1;
        } else {
            long long maxAge = std::chrono::duration_cast<std::chrono::milliseconds>(*_expirationTime - std::chrono::steady_clock::now()).count();
            return maxAge > 0 ? maxAge : 0;
        }
    }

    void TileData::setMaxAge(long long maxAge) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (maxAge < 0) {
            _expirationTime.reset();
        } else {
            _expirationTime = std::make_shared<std::chrono::steady_clock::time_point>(std::chrono::steady_clock::now() + std::chrono::milliseconds(maxAge));
        }
    }
    
    bool TileData::isReplaceWithParent() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _replaceWithParent;
    }
    
    void TileData::setReplaceWithParent(bool flag) {
        std::lock_guard<std::mutex> lock(_mutex);
        _replaceWithParent = flag;
    }
    
    bool TileData::isOverZoom() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _overzoom;
    }
    
    void TileData::setIsOverZoom(bool flag) {
        std::lock_guard<std::mutex> lock(_mutex);
        _overzoom = flag;
    }
    
    const std::shared_ptr<BinaryData>& TileData::getData() const {
        return _data;
    }
    
    std::shared_ptr<Variant> TileData::getMetadata(const std::string& key) const {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _metadata.find(key);
        if (it != _metadata.end()) {
            return it->second;
        }
        return std::shared_ptr<Variant>();
    }
    
    void TileData::setMetadata(const std::string& key, const std::shared_ptr<Variant>& value) {
        std::lock_guard<std::mutex> lock(_mutex);
        _metadata[key] = value;
    }

}
