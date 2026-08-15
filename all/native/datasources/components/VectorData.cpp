#include "VectorData.h"
#include "vectorelements/VectorElement.h"

namespace massif {
    
    VectorData::VectorData(std::vector<std::shared_ptr<VectorElement> > elements) :
        _elements(std::move(elements))
    {
    }

    VectorData::~VectorData() {
    }

    const std::vector<std::shared_ptr<VectorElement> >& VectorData::getElements() const {
        return _elements;
    }

}
