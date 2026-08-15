/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_LABELDRAWDATA_H_
#define _MASSIF_LABELDRAWDATA_H_

#include "renderers/drawdatas/BillboardDrawData.h"

namespace massif {
    class Label;
    class LabelStyle;
    class ViewState;
    
    class LabelDrawData : public BillboardDrawData {
    public:
        LabelDrawData(const Label& label, const LabelStyle& style, const Projection& projection, const std::shared_ptr<ProjectionSurface>& projectionSurface, const ViewState& viewState);
        virtual ~LabelDrawData();
    };
    
}

#endif
