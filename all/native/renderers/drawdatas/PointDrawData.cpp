#include "PointDrawData.h"
#include "graphics/Bitmap.h"
#include "geometry/PointGeometry.h"
#include "projections/Projection.h"
#include "styles/PointStyle.h"
#include "vectorelements/Point.h"
#include "utils/Const.h"
#include "utils/Log.h"

#include <utility>
#include <algorithm>

namespace carto {

    PointDrawData::PointDrawData(const PointGeometry& geometry, const PointStyle& style, const Projection& projection) :
        VectorElementDrawData(style.getColor()),
        _bitmap(style.getBitmap()),
        _clickScale(style.getClickSize() == -1 ? std::max(1.0f, 1 + (IDEAL_CLICK_SIZE - style.getSize()) * CLICK_SIZE_COEF / style.getSize()) : style.getClickSize() / style.getSize()),
        _pos(),
        _size(style.getSize())
    {
        MapPos posInternal = projection.toInternal(geometry.getPos());
        _pos = cglib::vec3<double>(posInternal.getX(), posInternal.getY(), posInternal.getZ());
    }
    
    PointDrawData::~PointDrawData() {
    }
    
    const std::shared_ptr<Bitmap> PointDrawData::getBitmap() const {
        return _bitmap;
    }
    
    float PointDrawData::getClickScale() const {
        return _clickScale;
    }
    
    const cglib::vec3<double>& PointDrawData::getPos() const {
        return _pos;
    }
    
    float PointDrawData::getSize() const {
        return _size;
    }
    
    void PointDrawData::offsetHorizontally(double offset) {
        _pos(0) += offset;
        setIsOffset(true);
    }
    
    const float PointDrawData::CLICK_SIZE_COEF = 0.5f;
    
}
