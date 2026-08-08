/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_FRAMEBUFFER_H_
#define _CARTO_FRAMEBUFFER_H_

#include "renderers/utils/GLResource.h"

#include <memory>
#include <vector>

namespace carto {
    
    class FrameBuffer : public GLResource {
    public:
        virtual ~FrameBuffer();
        
        int getWidth() const;
        int getHeight() const;

        bool isColor() const;
        bool isDepth() const;
        bool isStencil() const;

        GLuint getFBOId() const;
        GLuint getColorTexId() const;
        /**
         * The color texture currently attached to the framebuffer - the one drawing goes to,
         * which is not the primary texture while the secondary one is attached.
         */
        GLuint getAttachedColorTexId() const;
        /**
         * Attaches the secondary color texture (created on first use) or the primary one again.
         * The depth/stencil attachments are untouched, so a full-screen pass can read one color
         * texture and write the other while keeping the depth the scene was drawn with - which
         * is what lets content be drawn after a post-process effect and still be occluded by it.
         */
        void attachSecondaryColorTex(bool secondary);

        void discard(bool color, bool depth, bool stencil);

    protected:
        friend GLResourceManager;

        FrameBuffer(const std::weak_ptr<GLResourceManager>& manager, int width, int height, bool color, bool depth, bool stencil);

        virtual void create();
        virtual void destroy();

    private:
        int _width;
        int _height;
        bool _color;
        bool _depth;
        bool _stencil;
    
        GLuint _fboId;
        GLuint _colorTexId;
        GLuint _secondaryColorTexId;
        bool _secondaryAttached;
        std::vector<GLuint> _depthStencilRBIds;
    };
    
}

#endif
