/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_PELIASGEOCODINGPROXY_H_
#define _MASSIF_PELIASGEOCODINGPROXY_H_

#ifdef _MASSIF_GEOCODING_SUPPORT

#include "geocoding/GeocodingResult.h"

#include <memory>
#include <string>
#include <vector>

namespace massif {
    class Projection;
    
    class PeliasGeocodingProxy {
    public:
        static std::vector<std::shared_ptr<GeocodingResult> > ReadResponse(const std::string& responseString, const std::shared_ptr<Projection>& proj);

    private:
        PeliasGeocodingProxy();
    };
    
}

#endif

#endif
