#include "VectorTileFeature.h"

namespace massif {

    VectorTileFeature::VectorTileFeature(long long id, const MapTile& mapTile, const std::string& layerName, const std::shared_ptr<Geometry>& geometry, Variant properties) :
        Feature(geometry, std::move(properties)),
        _id(id),
        _mapTile(mapTile),
        _layerName(layerName),
        _distance(-1)
    {
    }
    
    VectorTileFeature::~VectorTileFeature() {
    }

    long long VectorTileFeature::getId() const {
        return _id;
    }

    const MapTile& VectorTileFeature::getMapTile() const {
        return _mapTile;
    }

    const std::string& VectorTileFeature::getLayerName() const {
        return _layerName;
    }
    
    double VectorTileFeature::getDistance() const {
        return _distance;
    }
    void VectorTileFeature::setDistance(double value) {
        _distance = value;
    }
}
