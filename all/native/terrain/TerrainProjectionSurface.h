/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_TERRAINPROJECTIONSURFACE_H_
#define _CARTO_TERRAINPROJECTIONSURFACE_H_

#include "projections/PlanarProjectionSurface.h"

#include <memory>

namespace carto {
    class ElevationManager;

    /**
     * A planar projection surface displaced by terrain elevation. Used when creating
     * vector element draw data so that elements are placed on top of the terrain
     * (the z coordinate of element positions is interpreted as height above terrain).
     * The elevation version is captured at construction time; VectorLayer creates a new
     * instance when the version changes, which triggers a draw data rebuild through the
     * existing projection-surface identity checks.
     * Internal class, not exposed in the public API.
     */
    class TerrainProjectionSurface : public PlanarProjectionSurface {
    public:
        explicit TerrainProjectionSurface(const std::shared_ptr<ElevationManager>& elevationManager);

        const std::shared_ptr<ElevationManager>& getElevationManager() const { return _elevationManager; }
        unsigned int getElevationVersion() const { return _elevationVersion; }

        virtual MapPos calculateMapPos(const cglib::vec3<double>& pos) const;
        virtual cglib::vec3<double> calculatePosition(const MapPos& mapPos) const;
        virtual cglib::vec3<double> calculateNormal(const MapPos& mapPos) const;
        virtual cglib::vec3<double> calculateVector(const MapPos& mapPos, const MapVec& mapVec) const;
        virtual cglib::vec3<double> calculateNearestPoint(const cglib::vec3<double>& pos, double height) const;
        virtual bool calculateHitPoint(const cglib::ray3<double>& ray, double height, double& t) const;

        virtual void tesselateSegment(const MapPos& mapPos0, const MapPos& mapPos1, std::vector<MapPos>& mapPoses) const;
        virtual void tesselateTriangle(unsigned int i0, unsigned int i1, unsigned int i2, std::vector<unsigned int>& indices, std::vector<MapPos>& mapPoses) const;

    private:
        bool splitSegment(const MapPos& mapPos0, const MapPos& mapPos1, MapPos& mapPosM) const;

        static double CalculateSplitThreshold(const std::shared_ptr<ElevationManager>& elevationManager);

        const std::shared_ptr<ElevationManager> _elevationManager;
        const unsigned int _elevationVersion;
        const double _splitThreshold; // internal units; longer segments/triangle edges are subdivided to follow the terrain
        const double _heightLift; // internal units; draped elements are lifted slightly above the terrain surface
    };
}

#endif
