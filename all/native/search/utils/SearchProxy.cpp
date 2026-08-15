#ifdef _MASSIF_SEARCH_SUPPORT

#include "SearchProxy.h"
#include "core/MapPos.h"
#include "core/Variant.h"
#include "components/Exceptions.h"
#include "geometry/Geometry.h"
#include "geometry/PointGeometry.h"
#include "geometry/LineGeometry.h"
#include "geometry/PolygonGeometry.h"
#include "geometry/MultiPointGeometry.h"
#include "geometry/MultiLineGeometry.h"
#include "geometry/MultiPolygonGeometry.h"
#include "geometry/MultiGeometry.h"
#include "search/query/QueryContext.h"
#include "search/query/QueryExpressionParser.h"
#include "projections/Projection.h"
#include "projections/EPSG3857.h"
#include "utils/Const.h"
#include "utils/Log.h"

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/variant.hpp>

#include <limits>
#include <algorithm>
#include <numeric>

namespace {

    typedef boost::geometry::model::d2::point_xy<double> BoostPointType;
    typedef boost::geometry::model::linestring<BoostPointType> BoostLinestringType;
    typedef boost::geometry::model::polygon<BoostPointType> BoostPolygonType;
    typedef boost::variant<BoostPointType, BoostLinestringType, BoostPolygonType> BoostGeometryType;

    massif::MapBounds convertToEPSG3857(const massif::MapBounds& mapBounds, const std::shared_ptr<massif::Projection>& proj) {
        if (std::dynamic_pointer_cast<massif::EPSG3857>(proj)) {
            return mapBounds;
        }

        massif::MapPos mapPos0 = mapBounds.getMin();
        massif::MapPos mapPos1 = mapBounds.getMax();

        massif::EPSG3857 epsg3857;
        massif::MapBounds epsg3857Bounds;
        epsg3857Bounds.expandToContain(epsg3857.fromWgs84(proj->toWgs84(mapPos0)));
        epsg3857Bounds.expandToContain(epsg3857.fromWgs84(proj->toWgs84(massif::MapPos(mapPos0.getX(), mapPos1.getY()))));
        epsg3857Bounds.expandToContain(epsg3857.fromWgs84(proj->toWgs84(mapPos1)));
        epsg3857Bounds.expandToContain(epsg3857.fromWgs84(proj->toWgs84(massif::MapPos(mapPos1.getX(), mapPos0.getY()))));
        return epsg3857Bounds;
    }

    std::shared_ptr<massif::Geometry> convertToEPSG3857(const std::shared_ptr<massif::Geometry>& geometry, const std::shared_ptr<massif::Projection>& proj) {
        if (std::dynamic_pointer_cast<massif::EPSG3857>(proj)) {
            return geometry;
        }

        massif::EPSG3857 epsg3857;
        if (auto pointGeometry = std::dynamic_pointer_cast<massif::PointGeometry>(geometry)) {
            massif::MapPos mapPos = epsg3857.fromWgs84(proj->toWgs84(pointGeometry->getPos()));
            return std::make_shared<massif::PointGeometry>(mapPos);
        } else if (auto lineGeometry = std::dynamic_pointer_cast<massif::LineGeometry>(geometry)) {
            std::vector<massif::MapPos> mapPoses = lineGeometry->getPoses();
            std::for_each(mapPoses.begin(), mapPoses.end(), [&epsg3857, &proj](massif::MapPos& mapPos) { mapPos = epsg3857.fromWgs84(proj->toWgs84(mapPos)); });
            return std::make_shared<massif::LineGeometry>(mapPoses);
        } else if (auto polygonGeometry = std::dynamic_pointer_cast<massif::PolygonGeometry>(geometry)) {
            std::vector<std::vector<massif::MapPos> > rings = polygonGeometry->getRings();
            for (std::vector<massif::MapPos>& ring : rings) {
                std::for_each(ring.begin(), ring.end(), [&epsg3857, &proj](massif::MapPos& mapPos) { mapPos = epsg3857.fromWgs84(proj->toWgs84(mapPos)); });
            }
            return std::make_shared<massif::PolygonGeometry>(rings);
        } else if (auto multiGeometry = std::dynamic_pointer_cast<massif::MultiGeometry>(geometry)) {
            std::vector<std::shared_ptr<massif::Geometry> > geometries;
            for (int i = 0; i < multiGeometry->getGeometryCount(); i++) {
                geometries.push_back(convertToEPSG3857(multiGeometry->getGeometry(i), proj));
            }
            return std::make_shared<massif::MultiGeometry>(geometries);
        } else {
            throw massif::GenericException("Unsupported geometry type");
        }
    }

    BoostGeometryType convertToBoostGeometry(const std::shared_ptr<massif::Geometry>& geometry) {
        if (auto pointGeometry = std::dynamic_pointer_cast<massif::PointGeometry>(geometry)) {
            massif::MapPos mapPos = pointGeometry->getPos();
            BoostPointType boostPoint(mapPos.getX(), mapPos.getY());
            return boostPoint;
        } else if (auto lineGeometry = std::dynamic_pointer_cast<massif::LineGeometry>(geometry)) {
            const std::vector<massif::MapPos>& mapPoses = lineGeometry->getPoses();
            BoostLinestringType boostLinestring;
            std::for_each(mapPoses.begin(), mapPoses.end(), [&boostLinestring](const massif::MapPos& mapPos) { boostLinestring.push_back(BoostPointType(mapPos.getX(), mapPos.getY())); });
            return boostLinestring;
        } else if (auto polygonGeometry = std::dynamic_pointer_cast<massif::PolygonGeometry>(geometry)) {
            const std::vector<std::vector<massif::MapPos> >& rings = polygonGeometry->getRings();
            BoostPolygonType boostPolygon;
            for (std::size_t i = 0; i < rings.size(); i++) {
                BoostPolygonType::ring_type boostRing;
                std::for_each(rings[i].begin(), rings[i].end(), [&boostRing](const massif::MapPos& mapPos) { boostRing.push_back(BoostPointType(mapPos.getX(), mapPos.getY())); });
                if (!boostRing.empty()) {
                    BoostPointType pos = boostRing.front();
                    boostRing.push_back(pos);
                }
                if (i == 0) {
                    boostPolygon.outer() = std::move(boostRing);
                } else {
                    boostPolygon.inners().push_back(std::move(boostRing));
                }
            }
            return boostPolygon;
        } else {
            throw massif::GenericException("Unsupported geometry type");
        }
    }

    bool matchRegexFilter(const massif::Variant& variant, const std::regex& re) {
        std::string str;
        switch (variant.getType()) {
        case massif::VariantType::VARIANT_TYPE_NULL:
            return false;
        case massif::VariantType::VARIANT_TYPE_STRING:
        case massif::VariantType::VARIANT_TYPE_BOOL:
        case massif::VariantType::VARIANT_TYPE_INTEGER:
        case massif::VariantType::VARIANT_TYPE_DOUBLE:
            return std::regex_match(variant.getString(), re);
        case massif::VariantType::VARIANT_TYPE_ARRAY:
            for (int i = 0; i < variant.getArraySize(); i++) {
                if (matchRegexFilter(variant.getArrayElement(i), re)) {
                    return true;
                }
            }
            return false;
        case massif::VariantType::VARIANT_TYPE_OBJECT:
            for (const std::string& key : variant.getObjectKeys()) {
                if (matchRegexFilter(variant.getObjectElement(key), re)) {
                    return true;
                }
            }
            return false;
        }
        return false;
    }

    class SearchQueryContext : public massif::QueryContext {
    public:
        explicit SearchQueryContext(const std::shared_ptr<massif::Geometry>& geometry, const std::string* layerName, const massif::Variant& var) : _geometry(geometry), _layerName(layerName), _variant(var) { }
        virtual ~SearchQueryContext() { }

        virtual bool getVariable(const std::string& name, massif::Variant& value) const {
            if (name == "layer::name") {
                value = (_layerName ? massif::Variant(*_layerName) : massif::Variant());
                return true;
            }

            if (name == "geometry::type") {
                value = massif::Variant(GetGeometryType(_geometry));
                return true;
            }

            if (name == "geometry::vertices") {
                value = massif::Variant(static_cast<long long>(GetGeometryVerticesCount(_geometry)));
                return true;
            }

            switch (_variant.getType()) {
            case massif::VariantType::VARIANT_TYPE_OBJECT:
                if (_variant.containsObjectKey(name)) {
                    value = _variant.getObjectElement(name);
                    return true;
                }
                return false;
            case massif::VariantType::VARIANT_TYPE_ARRAY:
                return false;
            default:
                if (name == "value") {
                    value = _variant;
                    return true;
                }
                return false;
            }
        }

    private:
        static std::string GetGeometryType(const std::shared_ptr<massif::Geometry>& geometry) {
            if (std::dynamic_pointer_cast<massif::PointGeometry>(geometry)) {
                return "point";
            } else if (std::dynamic_pointer_cast<massif::LineGeometry>(geometry)) {
                return "linestring";
            } else if (std::dynamic_pointer_cast<massif::PolygonGeometry>(geometry)) {
                return "polygon";
            } else if (std::dynamic_pointer_cast<massif::MultiPointGeometry>(geometry)) {
                return "multipoint";
            } else if (std::dynamic_pointer_cast<massif::MultiLineGeometry>(geometry)) {
                return "multilinestring";
            } else if (std::dynamic_pointer_cast<massif::MultiPolygonGeometry>(geometry)) {
                return "multipolygon";
            } else if (std::dynamic_pointer_cast<massif::MultiGeometry>(geometry)) {
                return "multigeometry";
            } else if (std::dynamic_pointer_cast<massif::Geometry>(geometry)) {
                return "geometry";
            }
            return "unknown";
        }
        
        static std::size_t GetGeometryVerticesCount(const std::shared_ptr<massif::Geometry>& geometry) {
            if (std::dynamic_pointer_cast<massif::PointGeometry>(geometry)) {
                return 1;
            } else if (auto lineGeometry = std::dynamic_pointer_cast<massif::LineGeometry>(geometry)) {
                return lineGeometry->getPoses().size();
            } else if (auto polygonGeometry = std::dynamic_pointer_cast<massif::PolygonGeometry>(geometry)) {
                return std::accumulate(polygonGeometry->getHoles().begin(), polygonGeometry->getHoles().end(), polygonGeometry->getPoses().size(), [](std::size_t size, const std::vector<massif::MapPos>& ring) { return size + ring.size(); });
            } else if (auto multiGeometry = std::dynamic_pointer_cast<massif::MultiGeometry>(geometry)) {
                std::size_t count = 0;
                for (int i = 0; i < multiGeometry->getGeometryCount(); i++) {
                    count += GetGeometryVerticesCount(multiGeometry->getGeometry(i));
                }
                return count;
            }
            return 0;
        }

        const std::shared_ptr<massif::Geometry>& _geometry;
        const std::string* _layerName;
        const massif::Variant& _variant;
    };

}

namespace massif {

    SearchProxy::SearchProxy(const std::shared_ptr<SearchRequest>& request, const MapBounds& mapBounds, const std::shared_ptr<Projection>& proj) :
        _request(request),
        _geometry(),
        _searchBounds(),
        _searchRadius(0),
        _projection(proj),
        _expr(),
        _re()
    {
        if (!request) {
            throw NullArgumentException("Null request");
        }
        if (!proj) {
            throw NullArgumentException("Null proj");
        }

        if (!request->getRegexFilter().empty()) {
            try {
                _re = std::regex(request->getRegexFilter());
            }
            catch (const std::exception& ex) {
                throw ParseException(std::string("Failed to parse regex: ") + ex.what(), request->getRegexFilter());
            }
        }

        if (!request->getFilterExpression().empty()) {
            try {
                _expr = QueryExpressionParser::parse(request->getFilterExpression());
            }
            catch (const std::exception& ex) {
                throw ParseException(std::string("Failed to parse expression: ") + ex.what(), request->getFilterExpression());
            }
        }

        if (request->getGeometry()) {
            if (!request->getProjection()) {
                throw NullArgumentException("Null projection while geometry is not null");
            }

            MapPos wgs84CenterPos = request->getProjection()->toWgs84(request->getGeometry()->getCenterPos());
            _geometry = convertToEPSG3857(request->getGeometry(), request->getProjection());
            _searchBounds = _geometry->getBounds();
            _searchRadius = request->getSearchRadius();
            if (_searchRadius >= 0) {
                _searchRadius = _searchRadius / std::cos(std::min(89.9, std::abs(wgs84CenterPos.getY())) * Const::DEG_TO_RAD);
                MapPos boundsPos0 = _searchBounds.getMin() - MapVec(_searchRadius, _searchRadius);
                MapPos boundsPos1 = _searchBounds.getMax() + MapVec(_searchRadius, _searchRadius);
                boundsPos0[0] = std::max(boundsPos0[0], EPSG3857().getBounds().getMin()[0] * 0.9999);
                boundsPos1[0] = std::min(boundsPos1[0], EPSG3857().getBounds().getMax()[0] * 0.9999);
                _searchBounds = MapBounds(boundsPos0, boundsPos1);
            }
            
        } else {
            _searchBounds = convertToEPSG3857(mapBounds, proj);
        }
    }

    double SearchProxy::calculateDistance(const std::shared_ptr<massif::Geometry>& geometry1, const std::shared_ptr<massif::Geometry>& geometry2) {
        if (auto multiGeometry1 = std::dynamic_pointer_cast<massif::MultiGeometry>(geometry1)) {
            double dist = std::numeric_limits<double>::infinity();
            for (int i = 0; i < multiGeometry1->getGeometryCount(); i++) {
                dist = std::min(dist, calculateDistance(multiGeometry1->getGeometry(i), geometry2));
            }
            return dist;
        }

        if (auto multiGeometry2 = std::dynamic_pointer_cast<massif::MultiGeometry>(geometry2)) {
            double dist = std::numeric_limits<double>::infinity();
            for (int i = 0; i < multiGeometry2->getGeometryCount(); i++) {
                dist = std::min(dist, calculateDistance(geometry1, multiGeometry2->getGeometry(i)));
            }
            return dist;
        }

        return boost::geometry::distance(convertToBoostGeometry(geometry1), convertToBoostGeometry(geometry2));
    }


    const MapBounds& SearchProxy::getSearchBounds() const {
        return _searchBounds;
    }

    bool SearchProxy::testBounds(const MapBounds& bounds) const {
        if (_searchRadius >= 0 && _geometry) {
            std::vector<MapPos> points(4);
            points[0] = bounds.getMin();
            points[1] = MapPos(bounds.getMin().getX(), bounds.getMax().getY());
            points[2] = bounds.getMax();
            points[3] = MapPos(bounds.getMax().getX(), bounds.getMin().getY());
            auto geometry = std::make_shared<PolygonGeometry>(std::move(points));
            double dist = calculateDistance(convertToEPSG3857(geometry, _projection), _geometry);
            if (dist > _searchRadius) {
                return false;
            }
        }

        return true;
    }

    double SearchProxy::testElement(const std::shared_ptr<Geometry>& geometry, const std::string* layerName, const Variant& var) const {
        double distance = 0;
        if (_geometry) {
            distance = calculateDistance(convertToEPSG3857(geometry, _projection), _geometry);
            if (_searchRadius > 0 && distance > _searchRadius) {
                return -1;
            }
        }
        if (_expr) {
            SearchQueryContext context(geometry, layerName, var);
            if (!_expr->evaluate(context)) {
                return -1;
            }
        }
        if (_re) {
            if (!matchRegexFilter(var, *_re)) {
                return -1;
            }
        }
        return distance;
    }

}

#endif
