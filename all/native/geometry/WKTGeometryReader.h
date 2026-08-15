/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_WKTGEOMETRYREADER_H_
#define _MASSIF_WKTGEOMETRYREADER_H_

#ifdef _MASSIF_WKBT_SUPPORT

#include <memory>
#include <string>

namespace massif {
    class Geometry;

    /**
     * A reader for Well Known Text representation of the geometry.
     */
    class WKTGeometryReader {
    public:
        /**
         * Constructs a WKTGeometryReader object.
         */
        WKTGeometryReader();

        /**
         * Reads geometry from the specified WKT string.
         * @param wkt The WKT string to read. For example, "POINT(1,1)"
         * @return The geometry read from the string. Null if reading failed.
         * @throws std::runtime_error If reading fails.
         */
        std::shared_ptr<Geometry> readGeometry(const std::string& wkt) const;
    };

}

#endif

#endif
