/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_SEARCHPROXY_H_
#define _MASSIF_SEARCHPROXY_H_

#ifdef _MASSIF_SEARCH_SUPPORT

#include "core/MapBounds.h"
#include "search/SearchRequest.h"

#include <memory>
#include <optional>
#include <vector>
#include <regex>

namespace massif {
    class Geometry;
    class Projection;
    class QueryExpression;
    class Variant;

    class SearchProxy {
    public:
        SearchProxy(const std::shared_ptr<SearchRequest>& request, const MapBounds& mapBounds, const std::shared_ptr<Projection>& proj);

        const MapBounds& getSearchBounds() const;

        bool testBounds(const MapBounds& bounds) const;

        double testElement(const std::shared_ptr<Geometry>& geometry, const std::string* layerName, const Variant& var) const;

        static double calculateDistance(const std::shared_ptr<massif::Geometry>& geometry1, const std::shared_ptr<massif::Geometry>& geometry2);

    protected:
        std::shared_ptr<SearchRequest> _request;
        std::shared_ptr<Geometry> _geometry;
        MapBounds _searchBounds;
        double _searchRadius;
        std::shared_ptr<Projection> _projection;
        std::shared_ptr<QueryExpression> _expr;
        std::optional<std::regex> _re;
    };
    
}

#endif

#endif
