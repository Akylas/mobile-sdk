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
        int tileMask = (1 << tileId.zoom) - 1;
        MapTile mapTile(tileId.x & tileMask, std::min(std::max(tileId.y, 0), tileMask), tileId.zoom, 0);
        long long mapTileId = mapTile.getTileId();

        // The provider is called once per tile per render pass; resolve the tile at most
        // once per frame and reuse the resolution for the remaining passes.
        long long gridTileId = -1;
        auto frameIt = _frameResolved.find(mapTileId);
        if (frameIt != _frameResolved.end()) {
            gridTileId = frameIt->second;
        } else {
            if (!resolveEntry(tileId, gridTileId)) {
                gridTileId = -1;
            }
            _frameResolved[mapTileId] = gridTileId;
        }
        if (gridTileId < 0) {
            return false;
        }

        auto it = _cache.find(gridTileId);
        if (it == _cache.end() || !it->second.texture || it->second.texture->getTexId() == 0) {
            return false;
        }
        it->second.lastUsed = ++_accessCounter;
        fillTexture(it->second, static_cast<float>(_elevationManager->getExaggeration() * Const::WORLD_SIZE / Const::EARTH_CIRCUMFERENCE), terrainTexture);
        return true;
    }

    bool ElevationTextureCache::resolveEntry(const vt::TileId& tileId, long long& gridTileId) {
        // Resolve the best already-decoded elevation grid for the tile. This mirrors the
        // CPU displacement path (TerrainTileTransformer), so the GPU-sampled heights stay
        // consistent with element placement, hit testing and label anchors.
        int tileMask = (1 << tileId.zoom) - 1;
        MapTile mapTile(tileId.x & tileMask, std::min(std::max(tileId.y, 0), tileMask), tileId.zoom, 0);

        // The tile that should carry this tile's elevation data: the tile's own level, capped by
        // the data source maximum zoom AND by the resolution the surface mesh can express. An
        // elevation tile is typically 256-512 texels while the mesh has MeshResolution cells, so
        // taking the level literally would give every render tile its own elevation texture for
        // detail that cannot be rendered - four times the decoded grids, texture uploads and tile
        // requests per level. One texel per half surface cell is the useful limit; the coarser
        // level also means neighbouring tiles share one texture, which is seamless by construction.
        MapTile levelTile = mapTile;
        for (int size = _gridSizeHint; size > 2 * _surfaceResolution && levelTile.getZoom() > 0; size /= 2) {
            levelTile = levelTile.getParent();
        }
        MapTile dataTile = _elevationManager->getDataTile(levelTile);

        std::shared_ptr<ElevationTileGrid> grid = _elevationManager->getTileGrid(dataTile, ElevationManager::LoadMode::CACHED_ONLY);
        if (grid && grid->getWidth() > 0) {
            _gridSizeHint = grid->getWidth();
        }
        if (!grid || !(grid->getTile() == dataTile)) {
            // Missing or resolved through a coarser ancestor: request the real thing, ahead of
            // any neighbour request. Until it arrives this tile is displaced by an ancestor grid
            // - which is a 2x2 average of its children, i.e. a different height field than the
            // neighbouring tiles that already have their own level - and the surface tears along
            // the shared edge. Loading the right level fast is what closes it.
            // No-op when elevation prefetching is disabled or the tile is already queued.
            _elevationManager->prefetchTileGrid(dataTile, 2);
        }
        if (!grid || grid->getWidth() < 1 || grid->getHeight() < 1) {
            return false;
        }

        // Fetch the neighbour grids: the texture gets a 1-texel border taken from them, so
        // adjacent tiles bilinearly interpolate across tile borders from identical texel
        // pairs - same-level DEM tile borders become seam-free. With seamless tile edges
        // enabled, coarser ancestor grids are accepted as well and sampled geographically
        // (ElevationTileGrid::encodeTextureWithBorders): not perfectly symmetric across a
        // level change, but real DEM data instead of a duplicated edge texel.
        bool seamless = _elevationManager->isSeamlessTileEdgesEnabled();
        const MapTile& gridTile = grid->getTile();
        int gridMask = (1 << gridTile.getZoom()) - 1;
        auto neighbourGrid = [&, this](int dx, int dy) -> std::shared_ptr<ElevationTileGrid> {
            int ny = gridTile.getY() + dy; // XYZ convention: y grows south, grid rows grow north
            if (ny < 0 || ny > gridMask) {
                return std::shared_ptr<ElevationTileGrid>();
            }
            MapTile neighbourTile((gridTile.getX() + dx) & gridMask, ny, gridTile.getZoom(), 0);
            std::shared_ptr<ElevationTileGrid> neighbour = _elevationManager->getTileGrid(neighbourTile, ElevationManager::LoadMode::CACHED_ONLY);
            if (!neighbour || !(neighbour->getTile() == neighbourTile)) {
                // Border texels want the real neighbour tile, but after every tile's own level:
                // a missing neighbour costs one texel of border accuracy, a missing own level
                // displaces the whole tile. Diagonal neighbours only fill the corner texel where
                // four tiles meet, so they come last.
                _elevationManager->prefetchTileGrid(neighbourTile, dx == 0 || dy == 0 ? 1 : 0);
            }
            if (neighbour && !(neighbour->getTile() == neighbourTile) && !seamless) {
                neighbour.reset(); // strict mode: only exact same-level neighbours
            }
            if (neighbour && neighbour->getTile() == gridTile) {
                neighbour.reset(); // our own grid covers the neighbour: the texture is already continuous there
            }
            return neighbour;
        };
        // order: W, E, S, N, SW, SE, NW, NE ('south' = smaller internal y = larger XYZ tile y)
        std::array<std::shared_ptr<ElevationTileGrid>, 8> neighbours = { {
            neighbourGrid(-1, 0), neighbourGrid(1, 0), neighbourGrid(0, 1), neighbourGrid(0, -1),
            neighbourGrid(-1, 1), neighbourGrid(1, 1), neighbourGrid(-1, -1), neighbourGrid(1, -1)
        } };

        gridTileId = gridTile.getTileId();
        auto it = _cache.find(gridTileId);
        if (it == _cache.end() || it->second.grid != grid || it->second.neighbours != neighbours) {
            if (_cache.size() >= MAX_CACHED_TEXTURES && it == _cache.end()) {
                // Evict the least-recently-used entry, NOT the whole cache. A full flush
                // re-encodes+re-uploads every texture whenever the working set exceeds the cap,
                // which stalls the render thread on fast multi-level zooms (the working set of
                // visible + neighbour DEM tiles briefly exceeds the cap). LRU keeps the warm set.
                // Entries already used in this frame are kept if possible: dropping one would
                // make the tile fall back to flat in its remaining render passes.
                auto lru = _cache.end();
                for (auto entryIt = _cache.begin(); entryIt != _cache.end(); entryIt++) {
                    if (entryIt->second.lastUsed > _frameStartCounter) {
                        continue;
                    }
                    if (lru == _cache.end() || entryIt->second.lastUsed < lru->second.lastUsed) {
                        lru = entryIt;
                    }
                }
                if (lru == _cache.end()) {
                    lru = std::min_element(_cache.begin(), _cache.end(), [](const std::pair<const long long, CacheEntry>& a, const std::pair<const long long, CacheEntry>& b) {
                        return a.second.lastUsed < b.second.lastUsed;
                    });
                }
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
        }
        it->second.lastUsed = ++_accessCounter;
        return it->second.texture && it->second.texture->getTexId() != 0;
    }

    void ElevationTextureCache::fillTexture(const CacheEntry& entry, float metersToInternal, vt::GLTileRenderer::TerrainTexture& terrainTexture) {
        // The texture covers the grid bounds extended by the 1-texel border
        const MapBounds& bounds = entry.grid->getInternalBounds();
        double texelX = (bounds.getMax().getX() - bounds.getMin().getX()) / entry.grid->getWidth();
        double texelY = (bounds.getMax().getY() - bounds.getMin().getY()) / entry.grid->getHeight();
        terrainTexture.textureId = entry.texture->getTexId();
        terrainTexture.textureSize = cglib::vec2<int>(entry.grid->getWidth() + 2, entry.grid->getHeight() + 2);
        terrainTexture.internalOrigin = cglib::vec2<double>(bounds.getMin().getX() - texelX, bounds.getMin().getY() - texelY);
        terrainTexture.internalSize = cglib::vec2<double>(bounds.getMax().getX() - bounds.getMin().getX() + 2 * texelX, bounds.getMax().getY() - bounds.getMin().getY() + 2 * texelY);
        terrainTexture.decode = cglib::vec4<float>(entry.decode[0], entry.decode[1], entry.decode[2], entry.decode[3]);
        terrainTexture.metersToInternal = metersToInternal;
        terrainTexture.mercatorYScale = static_cast<float>(2.0 * Const::PI / Const::WORLD_SIZE);
    }

    void ElevationTextureCache::beginFrame() {
        _frameResolved.clear();
        _frameStartCounter = _accessCounter;
    }

    void ElevationTextureCache::setSurfaceResolution(int resolution) {
        int value = std::max(1, resolution);
        if (_surfaceResolution != value) {
            _surfaceResolution = value;
            clear(); // the elevation level cap changes with it
        }
    }

    void ElevationTextureCache::clear() {
        _cache.clear();
        _frameResolved.clear();
    }
}
