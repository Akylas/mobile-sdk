#include "TerrainDrapeCache.h"
#include "renderers/utils/GLContext.h"

#include <algorithm>

namespace carto {

    const std::size_t TerrainDrapeCache::MAX_POOLED_TEXTURES = 32;
    // Tiles are NOT dropped the moment they leave the visible cover. A zoom or a pan walks the
    // cover back and forth over the same tiles, and re-acquiring means re-baking every layer of
    // every tile - the cost that made zooming stall. Keeping a generation of tiles alive turns
    // that into a cache hit.
    const std::size_t TerrainDrapeCache::MAX_ENTRIES = 160;

    bool TerrainDrapeCache::Key::operator < (const Key& other) const {
        if (stack != other.stack) {
            return stack < other.stack;
        }
        if (tileId.zoom != other.tileId.zoom) {
            return tileId.zoom < other.tileId.zoom;
        }
        if (tileId.x != other.tileId.x) {
            return tileId.x < other.tileId.x;
        }
        return tileId.y < other.tileId.y;
    }

    TerrainDrapeCache::TerrainDrapeCache() :
        _resolution(1024),
        _stackSignature(0),
        _frameBuffer(0),
        _entries(),
        _texturePool(),
        _frameCounter(0)
    {
    }

    TerrainDrapeCache::~TerrainDrapeCache() {
        // GL resources must be released explicitly via deleteResources() while the context is
        // current; the destructor may run after it is gone.
    }

    int TerrainDrapeCache::getResolution() const {
        return _resolution;
    }

    void TerrainDrapeCache::setResolution(int resolution) {
        int value = std::min(2048, std::max(128, resolution));
        if (value == _resolution) {
            return;
        }
        _resolution = value;
        // Every cached texture is the old size, so none of them can be reused.
        for (auto it = _entries.begin(); it != _entries.end(); it++) {
            GLuint texture = it->second.texture;
            glDeleteTextures(1, &texture);
        }
        _entries.clear();
        for (unsigned int texture : _texturePool) {
            GLuint tex = texture;
            glDeleteTextures(1, &tex);
        }
        _texturePool.clear();
    }

    void TerrainDrapeCache::setStackSignature(std::size_t signature) {
        if (signature == _stackSignature) {
            return;
        }
        _stackSignature = signature;
        // The textures are kept: a stale picture of the same ground is a better thing to show for
        // the two or three frames the re-bake takes than the flat fill dropping them would leave.
        // They just stop being trusted - re-baked with the blank-tile budget, and never copied
        // into another tile.
        for (auto it = _entries.begin(); it != _entries.end(); it++) {
            it->second.stale = it->second.baked || it->second.seeded;
            // A seed is a copy of other tiles' pictures, so a seed made from the old stack is old
            // content too, and unlike a bake it has no fingerprint to notice that with.
            it->second.seeded = false;
        }
    }

    bool TerrainDrapeCache::isStale(const vt::TileId& tileId, int stack) const {
        auto it = _entries.find(Key { tileId, stack });
        return it != _entries.end() && it->second.stale;
    }

    void TerrainDrapeCache::beginFrame() {
        _frameCounter++;
        for (auto it = _entries.begin(); it != _entries.end(); it++) {
            it->second.used = false;
        }
    }

    unsigned int TerrainDrapeCache::createTexture() {
        if (!_texturePool.empty()) {
            unsigned int texture = _texturePool.back();
            _texturePool.pop_back();
            return texture;
        }
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _resolution, _resolution, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        return texture;
    }

    unsigned int TerrainDrapeCache::acquire(const vt::TileId& tileId, int stack, std::size_t fingerprint, bool& needsBake, bool& hasContent) {
        Key key { tileId, stack };
        Entry& entry = _entries[key];
        if (entry.texture == 0) {
            entry.texture = createTexture();
            entry.baked = false;
            entry.seeded = false;
            entry.stale = false;
        }
        entry.used = true;
        entry.lastUsedFrame = _frameCounter;
        // A changed fingerprint means the layers covering this tile changed - a style layer
        // finished loading, or a proxy was replaced by its native tile - so the texture is stale.
        // Stale means baked from a stack that no longer exists. The fingerprint does not always
        // catch that: dropping a layer leaves the remaining layers' content - and their hashes -
        // unchanged for tiles the dropped layer had nothing in.
        needsBake = !entry.baked || entry.stale || entry.fingerprint != fingerprint;
        // A seeded texture is not a bake, but it does show this tile's ground - sampling it is
        // right, and it is the difference between a stand-in and a flat fill.
        hasContent = entry.baked || entry.seeded;
        return entry.texture;
    }

    void TerrainDrapeCache::markBaked(const vt::TileId& tileId, int stack, std::size_t fingerprint, std::size_t layerMask) {
        auto it = _entries.find(Key { tileId, stack });
        if (it != _entries.end()) {
            it->second.fingerprint = fingerprint;
            it->second.layerMask = layerMask;
            it->second.baked = true;
            it->second.seeded = false;
            it->second.stale = false;
        }
    }

    void TerrainDrapeCache::markSeeded(const vt::TileId& tileId, int stack) {
        auto it = _entries.find(Key { tileId, stack });
        if (it != _entries.end()) {
            it->second.seeded = true;
        }
    }

    bool TerrainDrapeCache::isBaked(const vt::TileId& tileId, int stack) const {
        auto it = _entries.find(Key { tileId, stack });
        return it != _entries.end() && it->second.baked;
    }

    std::size_t TerrainDrapeCache::bakedLayerMask(const vt::TileId& tileId, int stack) const {
        auto it = _entries.find(Key { tileId, stack });
        if (it == _entries.end() || !it->second.baked) {
            return 0;
        }
        return it->second.layerMask;
    }

    unsigned int TerrainDrapeCache::findBaked(const vt::TileId& tileId, int stack) {
        auto it = _entries.find(Key { tileId, stack });
        // A stale entry must never be a source: seeding or standing in with it copies the previous
        // stack's picture into tiles that never had it, and a seed carries no fingerprint, so the
        // old content then survives every check that would have replaced it.
        if (it == _entries.end() || !it->second.baked || it->second.stale) {
            return 0;
        }
        // Standing in for a tile that has no texture of its own IS use: without this the entry
        // looks idle to the eviction pass below while it is the only thing on screen.
        it->second.used = true;
        it->second.lastUsedFrame = _frameCounter;
        return it->second.texture;
    }

    unsigned int TerrainDrapeCache::getFrameBuffer() {
        if (_frameBuffer == 0) {
            GLuint fbo = 0;
            glGenFramebuffers(1, &fbo);
            _frameBuffer = fbo;
        }
        return _frameBuffer;
    }

    void TerrainDrapeCache::endFrame() {
        if (_entries.size() <= MAX_ENTRIES) {
            return; // keep unused tiles cached; they come back constantly while panning/zooming
        }
        // Over budget: evict the least recently used entries, never one used this frame.
        std::vector<std::pair<unsigned int, Key> > candidates;
        candidates.reserve(_entries.size());
        for (auto it = _entries.begin(); it != _entries.end(); it++) {
            if (!it->second.used) {
                candidates.emplace_back(it->second.lastUsedFrame, it->first);
            }
        }
        std::sort(candidates.begin(), candidates.end(), [](const std::pair<unsigned int, Key>& a, const std::pair<unsigned int, Key>& b) {
            return a.first < b.first;
        });
        std::size_t evictCount = _entries.size() - MAX_ENTRIES;
        for (std::size_t i = 0; i < candidates.size() && i < evictCount; i++) {
            auto it = _entries.find(candidates[i].second);
            if (it == _entries.end()) {
                continue;
            }
            if (_texturePool.size() < MAX_POOLED_TEXTURES) {
                _texturePool.push_back(it->second.texture);
            } else {
                GLuint texture = it->second.texture;
                glDeleteTextures(1, &texture);
            }
            _entries.erase(it);
        }
    }

    void TerrainDrapeCache::deleteResources() {
        for (auto it = _entries.begin(); it != _entries.end(); it++) {
            GLuint texture = it->second.texture;
            glDeleteTextures(1, &texture);
        }
        _entries.clear();
        for (unsigned int texture : _texturePool) {
            GLuint tex = texture;
            glDeleteTextures(1, &tex);
        }
        _texturePool.clear();
        if (_frameBuffer != 0) {
            GLuint fbo = _frameBuffer;
            glDeleteFramebuffers(1, &fbo);
            _frameBuffer = 0;
        }
    }

}
