#ifndef _CLUSTERELEMENTBUILDER_I
#define _CLUSTERELEMENTBUILDER_I

%module(directors="1") ClusterElementBuilder
!proxy_imports(massif::ClusterElementBuilder, core.MapPos, vectorelements.VectorElement, vectorelements.VectorElementVector)

%{
#include "layers/ClusterElementBuilder.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapPos.i"
%import "vectorelements/VectorElement.i"

!enum(massif::ClusterBuilderMode::ClusterBuilderMode)
!polymorphic_shared_ptr(massif::ClusterElementBuilder, layers.ClusterElementBuilder)

%feature("director") massif::ClusterElementBuilder;

%include "layers/ClusterElementBuilder.h"

#endif
