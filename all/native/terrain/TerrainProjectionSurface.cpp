#include "TerrainProjectionSurface.h"
#include "terrain/ElevationManager.h"

namespace carto {

    TerrainProjectionSurface::TerrainProjectionSurface(const std::shared_ptr<ElevationManager>& elevationManager) :
        PlanarProjectionSurface(),
        _elevationManager(elevationManager),
        _elevationVersion(elevationManager->getVersion())
    {
    }

    MapPos TerrainProjectionSurface::calculateMapPos(const cglib::vec3<double>& pos) const {
        double terrainZ = _elevationManager->getDisplayHeight(pos(0), pos(1), ElevationManager::LoadMode::CACHED_ONLY);
        return MapPos(pos(0), pos(1), pos(2) - terrainZ);
    }

    cglib::vec3<double> TerrainProjectionSurface::calculatePosition(const MapPos& mapPos) const {
        double terrainZ = _elevationManager->getDisplayHeight(mapPos.getX(), mapPos.getY(), ElevationManager::LoadMode::ALLOW_LOAD);
        return cglib::vec3<double>(mapPos.getX(), mapPos.getY(), mapPos.getZ() + terrainZ);
    }

    cglib::vec3<double> TerrainProjectionSurface::calculateNearestPoint(const cglib::vec3<double>& pos, double height) const {
        double terrainZ = _elevationManager->getDisplayHeight(pos(0), pos(1), ElevationManager::LoadMode::CACHED_ONLY);
        return cglib::vec3<double>(pos(0), pos(1), height + terrainZ);
    }

    bool TerrainProjectionSurface::calculateHitPoint(const cglib::ray3<double>& ray, double height, double& t) const {
        if (_elevationManager->intersectRay(ray, t)) {
            return true;
        }
        return PlanarProjectionSurface::calculateHitPoint(ray, height, t);
    }
}
