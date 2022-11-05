#ifndef _CLUSTERELEMENTBUILDER_I
#define _CLUSTERELEMENTBUILDER_I

%module(directors="1") ClusterElementBuilder
!proxy_imports(carto::ClusterElementBuilder, core.MapPos, vectorelements.VectorElement, vectorelements.VectorElementVector)

%{
#include "layers/ClusterElementBuilder.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "core/MapPos.i"
%import "vectorelements/VectorElement.i"

!enum(carto::ClusterBuilderMode::ClusterBuilderMode)
!polymorphic_shared_ptr(carto::ClusterElementBuilder, layers.ClusterElementBuilder)

%feature("director") carto::ClusterElementBuilder;

%include "layers/ClusterElementBuilder.h"

#endif
