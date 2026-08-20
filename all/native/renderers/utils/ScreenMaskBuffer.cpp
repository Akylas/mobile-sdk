#include "ScreenMaskBuffer.h"
#include "renderers/utils/GLContext.h"
#include "utils/Log.h"

#include <algorithm>

namespace massif {

    ScreenMaskBuffer::ScreenMaskBuffer(bool useDepth) :
        _useDepth(useDepth),
        _width(0),
        _height(0),
        _frameBuffer(0),
        _texture(0),
        _depthBuffer(0),
        _failed(false)
    {
    }

    ScreenMaskBuffer::~ScreenMaskBuffer() {
        // GL resources must be released explicitly via deleteResources() while the context is
        // current; the destructor may run after it is gone.
    }

    int ScreenMaskBuffer::getWidth() const {
        return _width;
    }

    int ScreenMaskBuffer::getHeight() const {
        return _height;
    }

    void ScreenMaskBuffer::setSize(int screenWidth, int screenHeight, int divisor) {
        int width = std::max(1, screenWidth / std::max(1, divisor));
        int height = std::max(1, screenHeight / std::max(1, divisor));
        if (width != _width || height != _height) {
            _width = width;
            _height = height;
            deleteResources();
            _failed = false;
        }
    }

    bool ScreenMaskBuffer::createResources() {
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
        // a pixel instead of the nearest one, and the mask holds the shadow of hidden ground. A
        // pass that reduces its draws with a blend equation instead does not need it.
        if (_useDepth) {
            glGenRenderbuffers(1, &_depthBuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, _depthBuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, _width, _height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        GLint prevFrameBuffer = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFrameBuffer);
        glGenFramebuffers(1, &_frameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _texture, 0);
        if (_useDepth) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthBuffer);
        }
        bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER, prevFrameBuffer);
        if (!complete) {
            deleteResources();
            _failed = true;
            Log::Errorf("ScreenMaskBuffer: no usable %d x %d mask - the effect that wanted it is skipped", _width, _height);
            return false;
        }
        return true;
    }

    unsigned int ScreenMaskBuffer::getTexture() {
        return createResources() ? _texture : 0;
    }

    bool ScreenMaskBuffer::beginPass() {
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
        if (_useDepth) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);
        } else {
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
        }
        // White = untouched, which is what a pixel neither shadowed nor near a wall contributes.
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(_useDepth ? (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) : GL_COLOR_BUFFER_BIT);
        return true;
    }

    void ScreenMaskBuffer::endPass(unsigned int previousFrameBuffer, int viewportWidth, int viewportHeight) {
        // Detach before anything samples it - see beginPass.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, previousFrameBuffer);
        glViewport(0, 0, viewportWidth, viewportHeight);
        glEnable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
    }

    bool ScreenMaskBuffer::beginPassRaw() {
        if (!createResources()) {
            return false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _texture, 0);
        glViewport(0, 0, _width, _height);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | (_useDepth ? GL_DEPTH_BUFFER_BIT : 0));
        return true;
    }

    void ScreenMaskBuffer::endPassRaw(unsigned int previousFrameBuffer, int viewportWidth, int viewportHeight) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, previousFrameBuffer);
        glViewport(0, 0, viewportWidth, viewportHeight);
    }

    void ScreenMaskBuffer::deleteResources() {
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
