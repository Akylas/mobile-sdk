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

        // Fetch the same-level neighbour grids: the texture gets a 1-texel border copied
        // from them, so adjacent tiles bilinearly interpolate across tile borders from
        // identical texel pairs - same-level DEM tile borders become seam-free. Only exact
        // same-level neighbours are used (ancestor fallbacks would not be symmetric).
        const MapTile& gridTile = grid->getTile();
        int gridMask = (1 << gridTile.getZoom()) - 1;
        auto neighbourGrid = [&, this](int dx, int dy) -> std::shared_ptr<ElevationTileGrid> {
            int ny = gridTile.getY() + dy; // XYZ convention: y grows south, grid rows grow north
            if (ny < 0 || ny > gridMask) {
                return std::shared_ptr<ElevationTileGrid>();
            }
            MapTile neighbourTile((gridTile.getX() + dx) & gridMask, ny, gridTile.getZoom(), 0);
            std::shared_ptr<ElevationTileGrid> neighbour = _elevationManager->getTileGrid(neighbourTile, ElevationManager::LoadMode::CACHED_ONLY);
            if (neighbour && !(neighbour->getTile() == neighbourTile)) {
                neighbour.reset();
            }
            return neighbour;
        };
        // order: W, E, S, N, SW, SE, NW, NE ('south' = smaller internal y = larger XYZ tile y)
        std::array<std::shared_ptr<ElevationTileGrid>, 8> neighbours = { {
            neighbourGrid(-1, 0), neighbourGrid(1, 0), neighbourGrid(0, 1), neighbourGrid(0, -1),
            neighbourGrid(-1, 1), neighbourGrid(1, 1), neighbourGrid(-1, -1), neighbourGrid(1, -1)
        } };

        long long gridTileId = gridTile.getTileId();
        auto it = _cache.find(gridTileId);
        bool needsCreate = (it == _cache.end() || it->second.grid != grid || it->second.neighbours != neighbours);
        if (needsCreate) {
            bool haveUsableTexture = (it != _cache.end() && it->second.texture && it->second.texture->getTexId() != 0);
            // Budget new-texture creation per frame: encodeTextureWithBorders + glTexImage2D
            // run on the render thread, and a fast zoom surfaces many new tiles at once - doing
            // them all in one frame stalls the render thread (the terrain-only zoom hang). Over
            // budget, render this tile flat this frame (or keep its slightly stale texture); it
            // gets a fresh texture on a later frame.
            if (_frameCreations >= MAX_CREATIONS_PER_FRAME) {
                if (!haveUsableTexture) {
                    return false;
                }
                // else: reuse the existing texture; do not re-encode/upload this frame
            } else {
                if (_cache.size() >= MAX_CACHED_TEXTURES && it == _cache.end()) {
                    // Evict the least-recently-used entry, NOT the whole cache. A full flush
                    // re-encodes+re-uploads every texture whenever the working set exceeds the cap,
                    // which stalls the render thread on fast multi-level zooms (the working set of
                    // visible + neighbour DEM tiles briefly exceeds the cap). LRU keeps the warm set.
                    auto lru = std::min_element(_cache.begin(), _cache.end(), [](const std::pair<const long long, CacheEntry>& a, const std::pair<const long long, CacheEntry>& b) {
                        return a.second.lastUsed < b.second.lastUsed;
                    });
                    if (lru != _cache.end()) {
                        _cache.erase(lru);
                    }
                }
                CacheEntry entry;
                entry.grid = grid;
                entry.neighbours = neighbours;
                std::vector<std::uint8_t> rgbaData;
                grid->encodeTextureWithBorders(neighbours, rgbaData, entry.decode);
                // The encoded rows are south-to-north, i.e. already bottom-up in the Bitmap
                // convention. Bitmap treats a POSITIVE stride as top-down input and flips the
                // rows - pass a negative stride so the data is taken as-is (a flipped texture
                // mirrors every tile's terrain north-south).
                auto bitmap = std::make_shared<Bitmap>(rgbaData.data(), grid->getWidth() + 2, grid->getHeight() + 2, ColorFormat::COLOR_FORMAT_RGBA, -static_cast<int>(4 * (grid->getWidth() + 2)));
                entry.texture = _glResourceManager->create<Texture>(bitmap, false, false); // no mipmaps, clamp to edge
                it = _cache.insert_or_assign(gridTileId, std::move(entry)).first;
                _frameCreations++;
            }
        }
        it->second.lastUsed = ++_accessCounter;
        const CacheEntry& entry = it->second;
        if (!entry.texture || entry.texture->getTexId() == 0) {
            return false;
        }

        // The texture covers the grid bounds extended by the 1-texel border
        const MapBounds& bounds = entry.grid->getInternalBounds();
        double texelX = (bounds.getMax().getX() - bounds.getMin().getX()) / entry.grid->getWidth();
        double texelY = (bounds.getMax().getY() - bounds.getMin().getY()) / entry.grid->getHeight();
        terrainTexture.textureId = entry.texture->getTexId();
        terrainTexture.textureSize = cglib::vec2<int>(entry.grid->getWidth() + 2, entry.grid->getHeight() + 2);
        terrainTexture.internalOrigin = cglib::vec2<double>(bounds.getMin().getX() - texelX, bounds.getMin().getY() - texelY);
        terrainTexture.internalSize = cglib::vec2<double>(bounds.getMax().getX() - bounds.getMin().getX() + 2 * texelX, bounds.getMax().getY() - bounds.getMin().getY() + 2 * texelY);
        terrainTexture.decode = cglib::vec4<float>(entry.decode[0], entry.decode[1], entry.decode[2], entry.decode[3]);
        terrainTexture.metersToInternal = static_cast<float>(_elevationManager->getExaggeration() * Const::WORLD_SIZE / Const::EARTH_CIRCUMFERENCE);
        terrainTexture.mercatorYScale = static_cast<float>(2.0 * Const::PI / Const::WORLD_SIZE);
        return true;
    }

    void ElevationTextureCache::beginFrame() {
        _frameCreations = 0;
    }

    void ElevationTextureCache::clear() {
        _cache.clear();
    }
}
