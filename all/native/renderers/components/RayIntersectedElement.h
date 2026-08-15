/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_RAYINTERSECTEDELEMENT_H_
#define _MASSIF_RAYINTERSECTEDELEMENT_H_

#include <memory>
#include <any>

#include <cglib/vec.h>

namespace massif {
    class Layer;

    class RayIntersectedElement {
    public:
        template <typename T>
        RayIntersectedElement(const std::shared_ptr<T>& element, const std::shared_ptr<Layer>& layer, const cglib::vec3<double>& hitPos, const cglib::vec3<double>& elementPos, bool is3DElement, int pointIndex) :
            _element(element),
            _layer(layer),
            _hitPos(hitPos),
            _elementPos(elementPos),
            _is3DElement(is3DElement),
            _pointIndex(pointIndex)
        {
        }
        template <typename T>
        RayIntersectedElement(const std::shared_ptr<T>& element, const std::shared_ptr<Layer>& layer, const cglib::vec3<double>& hitPos, const cglib::vec3<double>& elementPos, bool is3DElement) :
            _element(element),
            _layer(layer),
            _hitPos(hitPos),
            _elementPos(elementPos),
            _is3DElement(is3DElement),
            _pointIndex(0)
        {
        }

        virtual ~RayIntersectedElement();

        template <typename T>
        std::shared_ptr<T> getElement() const {
            if (auto element = std::any_cast<std::shared_ptr<T> >(&_element)) {
                return *element;
            }
            return std::shared_ptr<T>();
        }
    
        const std::shared_ptr<Layer>& getLayer() const;

        const cglib::vec3<double>& getHitPos() const;

        const cglib::vec3<double>& getElementPos() const;

        bool is3DElement() const;
        int getPointIndex() const;

        double getDistance(const cglib::vec3<double>& origin) const;
    
    private:
        std::any _element;
        std::shared_ptr<Layer> _layer;
        cglib::vec3<double> _hitPos;
        cglib::vec3<double> _elementPos;
        bool _is3DElement;
        int _pointIndex;
    };
    
}

#endif
