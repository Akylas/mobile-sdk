#ifndef _VECTORDATA_I
#define _VECTORDATA_I

%module VectorData

!proxy_imports(massif::VectorData, vectorelements.VectorElement, vectorelements.VectorElementVector)

%{
#include "datasources/components/VectorData.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "vectorelements/VectorElement.i"

!shared_ptr(massif::VectorData, datasources.components.VectorData)

%attributeval(massif::VectorData, %arg(std::vector<std::shared_ptr<massif::VectorElement> >), Elements, getElements)
!standard_equals(massif::VectorData);

%include "datasources/components/VectorData.h"

#endif
