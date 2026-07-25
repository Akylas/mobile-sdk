#include "TerrainDrapeCache.h"
#include "renderers/utils/GLContext.h"

#include <algorithm>

namespace carto {

    const std::size_t TerrainDrapeCache::MAX_POOLED_TEXTURES = 32;

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
        _frameBuffer(0),
        _entries(),
        _texturePool()
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

    void TerrainDrapeCache::beginFrame() {
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

    unsigned int TerrainDrapeCache::acquire(const vt::TileId& tileId, int stack, std::size_t fingerprint, bool& needsBake) {
        Key key { tileId, stack };
        Entry& entry = _entries[key];
        if (entry.texture == 0) {
            entry.texture = createTexture();
            entry.baked = false;
        }
        entry.used = true;
        // A changed fingerprint means the layers covering this tile changed - a style layer
        // finished loading, or a proxy was replaced by its native tile - so the texture is stale.
        needsBake = !entry.baked || entry.fingerprint != fingerprint;
        if (needsBake) {
            entry.fingerprint = fingerprint;
            entry.baked = true;
        }
        return entry.texture;
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
        for (auto it = _entries.begin(); it != _entries.end(); ) {
            if (it->second.used) {
                it++;
                continue;
            }
            if (_texturePool.size() < MAX_POOLED_TEXTURES) {
                _texturePool.push_back(it->second.texture);
            } else {
                GLuint texture = it->second.texture;
                glDeleteTextures(1, &texture);
            }
            it = _entries.erase(it);
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
