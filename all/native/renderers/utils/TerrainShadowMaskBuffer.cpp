#include "TerrainShadowMaskBuffer.h"
#include "renderers/utils/GLContext.h"
#include "utils/Log.h"

#include <algorithm>

namespace carto {

    TerrainShadowMaskBuffer::TerrainShadowMaskBuffer() :
        _width(0),
        _height(0),
        _frameBuffer(0),
        _texture(0),
        _depthBuffer(0),
        _failed(false)
    {
    }

    TerrainShadowMaskBuffer::~TerrainShadowMaskBuffer() {
        // GL resources must be released explicitly via deleteResources() while the context is
        // current; the destructor may run after it is gone.
    }

    int TerrainShadowMaskBuffer::getWidth() const {
        return _width;
    }

    int TerrainShadowMaskBuffer::getHeight() const {
        return _height;
    }

    void TerrainShadowMaskBuffer::setSize(int screenWidth, int screenHeight, int divisor) {
        int width = std::max(1, screenWidth / std::max(1, divisor));
        int height = std::max(1, screenHeight / std::max(1, divisor));
        if (width != _width || height != _height) {
            _width = width;
            _height = height;
            deleteResources();
            _failed = false;
        }
    }

    bool TerrainShadowMaskBuffer::createResources() {
        if (_frameBuffer != 0) {
            return true;
        }
        if (_failed || _width <= 0 || _height <= 0) {
            return false;
        }
        glGenTextures(1, &_texture);
        glBindTexture(GL_TEXTURE_2D, _texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _width, _height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        // LINEAR: the mask holds a shadow FACTOR, which does interpolate - and interpolating it is
        // what keeps a half-resolution mask from showing its own texels along a shadow edge.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Depth, because the terrain tiles overlap on screen: without it the last tile drawn wins
        // a pixel instead of the nearest one, and the mask holds the shadow of hidden ground.
        glGenRenderbuffers(1, &_depthBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, _depthBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, _width, _height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        GLint prevFrameBuffer = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFrameBuffer);
        glGenFramebuffers(1, &_frameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _texture, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthBuffer);
        bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER, prevFrameBuffer);
        if (!complete) {
            deleteResources();
            _failed = true;
            Log::Errorf("TerrainShadowMaskBuffer: no usable %d x %d mask - the surface falls back to the analytic lookup", _width, _height);
            return false;
        }
        return true;
    }

    unsigned int TerrainShadowMaskBuffer::getTexture() {
        return createResources() ? _texture : 0;
    }

    bool TerrainShadowMaskBuffer::beginPass() {
        if (!createResources()) {
            return false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);
        // Re-attached per pass: it is DETACHED at the end, because a texture left attached to a
        // framebuffer still counts as a render target, and sampling one in the same frame is
        // undefined - on this driver it serialises the draws that sample it instead.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _texture, 0);
        glViewport(0, 0, _width, _height);
        glDisable(GL_BLEND);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_CULL_FACE); // displaced surfaces can face away near ridge crests
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        // White = fully lit, which is what a pixel with no terrain in it must contribute.
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return true;
    }

    void TerrainShadowMaskBuffer::endPass(unsigned int previousFrameBuffer, int viewportWidth, int viewportHeight) {
        // Detach before anything samples it - see beginPass.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, previousFrameBuffer);
        glViewport(0, 0, viewportWidth, viewportHeight);
        glEnable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
    }

    void TerrainShadowMaskBuffer::deleteResources() {
        if (_frameBuffer != 0) {
            glDeleteFramebuffers(1, &_frameBuffer);
            _frameBuffer = 0;
        }
        if (_texture != 0) {
            glDeleteTextures(1, &_texture);
            _texture = 0;
        }
        if (_depthBuffer != 0) {
            glDeleteRenderbuffers(1, &_depthBuffer);
            _depthBuffer = 0;
        }
    }

}
