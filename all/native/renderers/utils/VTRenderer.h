/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_VTRENDERER_H_
#define _MASSIF_VTRENDERER_H_

#include "renderers/utils/GLResource.h"

#include <memory>

#include <vt/GLTileRenderer.h>

namespace massif {
    
    class VTRenderer : public GLResource {
    public:
        virtual ~VTRenderer();

        std::shared_ptr<vt::GLTileRenderer> getTileRenderer() const;

    protected:
        friend GLResourceManager;

        VTRenderer(const std::weak_ptr<GLResourceManager>& manager, const std::shared_ptr<vt::TileTransformer>& tileTransformer);

        virtual void create();
        virtual void destroy();

    private:
        const std::shared_ptr<vt::TileTransformer> _tileTransformer;

        std::shared_ptr<vt::GLTileRenderer> _tileRenderer;
    };

}

#endif
