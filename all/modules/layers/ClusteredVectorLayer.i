#ifndef _CLUSTEREDVECTORLAYER_I
#define _CLUSTEREDVECTORLAYER_I

%module ClusteredVectorLayer

!proxy_imports(carto::ClusteredVectorLayer, datasources.LocalVectorDataSource, layers.VectorLayer, vectorelements.VectorElement, layers.ClusterElementBuilder)

%{
#include "layers/ClusteredVectorLayer.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "datasources/LocalVectorDataSource.i"
%import "layers/VectorLayer.i"
%import "layers/ClusterElementBuilder.i"

!polymorphic_shared_ptr(carto::ClusteredVectorLayer, layers.ClusteredVectorLayer)

%attribute(carto::ClusteredVectorLayer, float, MinimumClusterDistance, getMinimumClusterDistance, setMinimumClusterDistance)
%attribute(carto::ClusteredVectorLayer, float, MaximumClusterZoom, getMaximumClusterZoom, setMaximumClusterZoom)
%attribute(carto::ClusteredVectorLayer, bool, AnimatedClusters, isAnimatedClusters, setAnimatedClusters)
!attributestring_polymorphic(carto::ClusteredVectorLayer, layers.ClusterElementBuilder, ClusterElementBuilder, getClusterElementBuilder)
%std_exceptions(carto::ClusteredVectorLayer::ClusteredVectorLayer)

%include "layers/ClusteredVectorLayer.h"

#endif
