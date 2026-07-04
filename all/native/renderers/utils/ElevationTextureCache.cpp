#include "ElevationTextureCache.h"
#include "core/MapBounds.h"
#include "core/MapTile.h"
#include "graphics/Bitmap.h"
#include "renderers/utils/GLResourceManager.h"
#include "renderers/utils/Texture.h"
#include "terrain/ElevationManager.h"
#include "terrain/ElevationTileGrid.h"
#include "utils/Const.h"

#include <algorithm>
#include <vector>

namespace carto {

    ElevationTextureCache::ElevationTextureCache(const std::shared_ptr<ElevationManager>& elevationManager, const std::shared_ptr<GLResourceManager>& glResourceManager) :
        _elevationManager(elevationManager),
        _glResourceManager(glResourceManager),
        _cache()
    {
    }

    bool ElevationTextureCache::getTexture(const vt::TileId& tileId, vt::GLTileRenderer::TerrainTexture& terrainTexture) {
        // Resolve the best already-decoded elevation grid for the tile. This mirrors the
        // CPU displacement path (TerrainTileTransformer), so the GPU-sampled heights stay
        // consistent with element placement, hit testing and label anchors.
        int tileMask = (1 << tileId.zoom) - 1;
        MapTile mapTile(tileId.x & tileMask, std::min(std::max(tileId.y, 0), tileMask), tileId.zoom, 0);
        std::shared_ptr<ElevationTileGrid> grid = _elevationManager->getTileGrid(mapTile, ElevationManager::LoadMode::CACHED_ONLY);
        if (!grid || grid->getWidth() < 1 || grid->getHeight() < 1) {
            return false;
        }

        long long gridTileId = grid->getTile().getTileId();
        auto it = _cache.find(gridTileId);
        if (it == _cache.end() || it->second.grid != grid) {
            if (_cache.size() >= MAX_CACHED_TEXTURES) {
                _cache.clear(); // simple full flush; textures are cheap to rebuild from cached grids
            }
            CacheEntry entry;
            entry.grid = grid;
            std::vector<std::uint8_t> rgbaData;
            grid->encodeTexture(rgbaData, entry.decode);
            // The encoded rows are south-to-north, i.e. already bottom-up in the Bitmap
            // convention. Bitmap treats a POSITIVE stride as top-down input and flips the
            // rows - pass a negative stride so the data is taken as-is (a flipped texture
            // mirrors every tile's terrain north-south).
            auto bitmap = std::make_shared<Bitmap>(rgbaData.data(), grid->getWidth(), grid->getHeight(), ColorFormat::COLOR_FORMAT_RGBA, -static_cast<int>(4 * grid->getWidth()));
            entry.texture = _glResourceManager->create<Texture>(bitmap, false, false); // no mipmaps, clamp to edge
            it = _cache.insert_or_assign(gridTileId, std::move(entry)).first;
        }
        const CacheEntry& entry = it->second;
        if (!entry.texture || entry.texture->getTexId() == 0) {
            return false;
        }

        const MapBounds& bounds = entry.grid->getInternalBounds();
        terrainTexture.textureId = entry.texture->getTexId();
        terrainTexture.internalOrigin = cglib::vec2<double>(bounds.getMin().getX(), bounds.getMin().getY());
        terrainTexture.internalSize = cglib::vec2<double>(bounds.getMax().getX() - bounds.getMin().getX(), bounds.getMax().getY() - bounds.getMin().getY());
        terrainTexture.decode = cglib::vec4<float>(entry.decode[0], entry.decode[1], entry.decode[2], entry.decode[3]);
        terrainTexture.metersToInternal = static_cast<float>(_elevationManager->getExaggeration() * Const::WORLD_SIZE / Const::EARTH_CIRCUMFERENCE);
        terrainTexture.mercatorYScale = static_cast<float>(2.0 * Const::PI / Const::WORLD_SIZE);
        return true;
    }

    void ElevationTextureCache::clear() {
        _cache.clear();
    }
}
