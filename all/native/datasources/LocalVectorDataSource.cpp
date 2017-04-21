#include "LocalVectorDataSource.h"
#include "components/Exceptions.h"
#include "geometry/Geometry.h"
#include "geometry/PointGeometry.h"
#include "geometry/LineGeometry.h"
#include "geometry/PolygonGeometry.h"
#include "geometry/MultiGeometry.h"
#include "geometry/GeometrySimplifier.h"
#include "geometry/utils/KDTreeSpatialIndex.h"
#include "geometry/utils/NullSpatialIndex.h"
#include "vectorelements/Point.h"
#include "vectorelements/Line.h"
#include "vectorelements/Polygon.h"
#include "vectorelements/Polygon3D.h"
#include "vectorelements/GeometryCollection.h"
#include "renderers/components/CullState.h"
#include "utils/Log.h"

#include <algorithm>
#include <iterator>
#include <unordered_set>

namespace carto {
    
    LocalVectorDataSource::LocalVectorDataSource(const std::shared_ptr<Projection>& projection) :
        VectorDataSource(projection),
        _geometrySimplifier(),
        _spatialIndex(std::make_shared<NullSpatialIndex<std::shared_ptr<VectorElement> > >()),
        _elementId(0),
        _mutex()
    {
    }
    
    LocalVectorDataSource::LocalVectorDataSource(const std::shared_ptr<Projection>& projection, LocalSpatialIndexType::LocalSpatialIndexType spatialIndexType) :
        VectorDataSource(projection),
        _geometrySimplifier(),
        _spatialIndex(),
        _elementId(0),
        _mutex()
    {
        switch (spatialIndexType) {
            case LocalSpatialIndexType::LOCAL_SPATIAL_INDEX_TYPE_KDTREE:
                _spatialIndex = std::make_shared<KDTreeSpatialIndex<std::shared_ptr<VectorElement> > >();
                break;
            default:
                _spatialIndex = std::make_shared<NullSpatialIndex<std::shared_ptr<VectorElement> > >();
                break;
        }
    }
    
    LocalVectorDataSource::~LocalVectorDataSource() {
    }
    
    std::shared_ptr<VectorData> LocalVectorDataSource::loadElements(const std::shared_ptr<CullState>& cullState) {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::shared_ptr<VectorElement> > elements = _spatialIndex->query(cullState->getViewState().getFrustum());
        
        // If geometry simplifier is specified, create new vector elements with simplified geometry
        if (_geometrySimplifier) {
            float simplifierScale = calculateGeometrySimplifierScale(cullState->getViewState());

            std::vector<std::shared_ptr<VectorElement> > simplifiedElements;
            simplifiedElements.reserve(elements.size());
            for (const std::shared_ptr<VectorElement>& element : elements) {
                std::shared_ptr<VectorElement> simplifiedElement = simplifyElement(element, simplifierScale);
                if (simplifiedElement) {
                    simplifiedElements.emplace_back(std::move(simplifiedElement));
                }
            }
            std::swap(elements, simplifiedElements);
        }

        return std::make_shared<VectorData>(elements);
    }

    void LocalVectorDataSource::clear() {
        std::vector<std::shared_ptr<VectorElement> > removedElements;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            removedElements = _spatialIndex->getAll();
            _spatialIndex->clear();
        }
        if (!removedElements.empty()) {
            notifyElementsRemoved(removedElements);
        }
    }
    
    std::vector<std::shared_ptr<VectorElement> > LocalVectorDataSource::getAll() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _spatialIndex->getAll();
    }
    
    void LocalVectorDataSource::setAll(const std::vector<std::shared_ptr<VectorElement> >& elements) {
        for (const std::shared_ptr<VectorElement>& element : elements) {
            if (!element) {
                throw NullArgumentException("Null element");
            }
            if (auto dataSource = getElementDataSource(element)) {
                if (dataSource != shared_from_this()) {
                    throw InvalidArgumentException("Element already attached to a different datasource");
                }
            }
        }

        std::vector<std::shared_ptr<VectorElement> > elementsAdded, elementsRemoved;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            std::vector<std::shared_ptr<VectorElement> > oldElements = _spatialIndex->getAll();
            std::unordered_set<std::shared_ptr<VectorElement> > oldElementSet(oldElements.begin(), oldElements.end());
            
            // Rebuild spatial index, create list of added and removed elements
            _spatialIndex->clear();
            _spatialIndex->reserve(elements.size());
            for (const std::shared_ptr<VectorElement>& element : elements) {
                const MapBounds& bounds = element->getBounds();
                MapBounds internalBounds(_projection->toInternal(bounds.getMin()), _projection->toInternal(bounds.getMax()));
                auto it = oldElementSet.find(element);
                if (it != oldElementSet.end()) {
                    oldElementSet.erase(it);
                } else {
                    element->setId(_elementId);
                    elementsAdded.push_back(element);
                    _elementId++;
                }
                _spatialIndex->insert(internalBounds, element);
            }
            std::copy(oldElementSet.begin(), oldElementSet.end(), std::back_inserter(elementsRemoved));
        }
        if (!elementsAdded.empty()) {
            notifyElementsAdded(elementsAdded);
        }
        if (!elementsRemoved.empty()) {
            notifyElementsRemoved(elementsRemoved);
        }
    }
    
    void LocalVectorDataSource::add(const std::shared_ptr<VectorElement>& element) {
        if (!element) {
            throw NullArgumentException("Null element");
        }
        if (getElementDataSource(element)) {
            throw InvalidArgumentException("Element already attached to a datasource");
        }

        {
            std::lock_guard<std::mutex> lock(_mutex);
            element->setId(_elementId);
            const MapBounds& bounds = element->getBounds();
            MapBounds internalBounds(_projection->toInternal(bounds.getMin()), _projection->toInternal(bounds.getMax()));
            _spatialIndex->insert(internalBounds, element);
            _elementId++;
        }
        notifyElementAdded(element);
    }
    
    void LocalVectorDataSource::addAll(const std::vector<std::shared_ptr<VectorElement> >& elements) {
        for (const std::shared_ptr<VectorElement>& element : elements) {
            if (!element) {
                throw NullArgumentException("Null element");
            }
            if (getElementDataSource(element)) {
                throw InvalidArgumentException("Element already attached to a datasource");
            }
        }

        {
            std::lock_guard<std::mutex> lock(_mutex);
            _spatialIndex->reserve(_spatialIndex->size() + elements.size());
            for (const std::shared_ptr<VectorElement>& element : elements) {
                element->setId(_elementId);
                const MapBounds& bounds = element->getBounds();
                MapBounds internalBounds(_projection->toInternal(bounds.getMin()), _projection->toInternal(bounds.getMax()));
                _spatialIndex->insert(internalBounds, element);
                _elementId++;
            }
        }
        if (!elements.empty()) {
            notifyElementsAdded(elements);
        }
    }
    
    bool LocalVectorDataSource::remove(const std::shared_ptr<VectorElement>& element) {
        if (!element) {
            throw NullArgumentException("Null element");
        }
        if (auto dataSource = getElementDataSource(element)) {
            if (dataSource != shared_from_this()) {
                throw InvalidArgumentException("Element attached to a different datasource");
            }
        }

        bool removed = false;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            const MapBounds& bounds = element->getBounds();
            MapBounds internalBounds(_projection->toInternal(bounds.getMin()), _projection->toInternal(bounds.getMax()));
            removed = _spatialIndex->remove(internalBounds, element);
        }
        if (removed) {
            notifyElementRemoved(element);
        }
        return removed;
    }
    
    bool LocalVectorDataSource::removeAll(const std::vector<std::shared_ptr<VectorElement> >& elements) {
        for (const std::shared_ptr<VectorElement>& element : elements) {
            if (!element) {
                throw NullArgumentException("Null element");
            }
            if (auto dataSource = getElementDataSource(element)) {
                if (dataSource != shared_from_this()) {
                    throw InvalidArgumentException("Element attached to a different datasource");
                }
            }
        }

        std::vector<std::shared_ptr<VectorElement> > removedElements;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            for (const std::shared_ptr<VectorElement>& element : elements) {
                const MapBounds& bounds = element->getBounds();
                MapBounds internalBounds(_projection->toInternal(bounds.getMin()), _projection->toInternal(bounds.getMax()));
                if (_spatialIndex->remove(internalBounds, element)) {
                    removedElements.push_back(element);
                }
            }
        }
        if (!removedElements.empty()) {
            notifyElementsRemoved(removedElements);
        }
        return removedElements.size() == elements.size();
    }
    
    std::shared_ptr<GeometrySimplifier> LocalVectorDataSource::getGeometrySimplifier() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _geometrySimplifier;
    }
    
    void LocalVectorDataSource::setGeometrySimplifier(const std::shared_ptr<GeometrySimplifier>& simplifier) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _geometrySimplifier = simplifier;
        }
        notifyElementsChanged();
    }
    
    MapBounds LocalVectorDataSource::getDataExtent() const {
        std::lock_guard<std::mutex> lock(_mutex);
        MapBounds mapBounds;
        for (const std::shared_ptr<VectorElement>& element : _spatialIndex->getAll()) {
            const MapPos& p0 = element->getBounds().getMin();
            const MapPos& p1 = element->getBounds().getMax();
            mapBounds.expandToContain(MapPos(p0.getX(), p0.getY()));
            mapBounds.expandToContain(MapPos(p1.getX(), p0.getY()));
            mapBounds.expandToContain(MapPos(p1.getX(), p1.getY()));
            mapBounds.expandToContain(MapPos(p0.getX(), p1.getY()));
        }
        return mapBounds;
    }
    
    std::shared_ptr<VectorElement> LocalVectorDataSource::simplifyElement(const std::shared_ptr<VectorElement>& element, float scale) const {
        std::shared_ptr<VectorElement> simplifiedElement = element;
        if (auto lineElement = std::dynamic_pointer_cast<Line>(element)) {
            auto lineGeometry = std::dynamic_pointer_cast<LineGeometry>(lineElement->getGeometry());
            lineGeometry = std::dynamic_pointer_cast<LineGeometry>(_geometrySimplifier->simplify(lineGeometry, scale));
            if (lineGeometry) {
                simplifiedElement = std::make_shared<Line>(lineGeometry, lineElement->getStyle());
            } else {
                simplifiedElement.reset();
            }
        } else if (auto polygonElement = std::dynamic_pointer_cast<Polygon>(element)) {
            auto polygonGeometry = std::dynamic_pointer_cast<PolygonGeometry>(polygonElement->getGeometry());
            polygonGeometry = std::dynamic_pointer_cast<PolygonGeometry>(_geometrySimplifier->simplify(polygonGeometry, scale));
            if (polygonGeometry) {
                simplifiedElement = std::make_shared<Polygon>(polygonGeometry, polygonElement->getStyle());
            } else {
                simplifiedElement.reset();
            }
        } else if (auto polygon3DElement = std::dynamic_pointer_cast<Polygon3D>(element)) {
            auto polygonGeometry = std::dynamic_pointer_cast<PolygonGeometry>(polygon3DElement->getGeometry());
            polygonGeometry = std::dynamic_pointer_cast<PolygonGeometry>(_geometrySimplifier->simplify(polygonGeometry, scale));
            if (polygonGeometry) {
                simplifiedElement = std::make_shared<Polygon3D>(polygonGeometry, polygon3DElement->getStyle(), polygon3DElement->getHeight());
            } else {
                simplifiedElement.reset();
            }
        } else if (auto geomCollectionElement = std::dynamic_pointer_cast<GeometryCollection>(element)) {
            auto multiGeometry = std::dynamic_pointer_cast<MultiGeometry>(geomCollectionElement->getGeometry());
            multiGeometry = std::dynamic_pointer_cast<MultiGeometry>(_geometrySimplifier->simplify(multiGeometry, scale));
            if (multiGeometry) {
                simplifiedElement = std::make_shared<GeometryCollection>(multiGeometry, geomCollectionElement->getStyle());
            } else {
                simplifiedElement.reset();
            }
        }

        if (simplifiedElement != element) {
            // Copy base attributes. Note: we will not attach the new element to the data source, it is temporary object
            simplifiedElement->setId(element->getId());
            simplifiedElement->setMetaData(element->getMetaData());
            simplifiedElement->setVisible(element->isVisible());
        }

        return simplifiedElement;
    }

    void LocalVectorDataSource::notifyElementChanged(const std::shared_ptr<VectorElement>& element) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!(std::dynamic_pointer_cast<NullSpatialIndex<std::shared_ptr<VectorElement> > >(_spatialIndex))) {
                _spatialIndex->remove(element);
                const MapBounds& bounds = element->getBounds();
                MapBounds internalBounds(_projection->toInternal(bounds.getMin()), _projection->toInternal(bounds.getMax()));
                _spatialIndex->insert(internalBounds, element);
            }
        }
        VectorDataSource::notifyElementChanged(element);
    }
    
}
