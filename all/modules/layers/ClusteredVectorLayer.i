#ifndef _CLUSTEREDVECTORLAYER_I
#define _CLUSTEREDVECTORLAYER_I

%module ClusteredVectorLayer

!proxy_imports(massif::ClusteredVectorLayer, datasources.LocalVectorDataSource, layers.VectorLayer, vectorelements.VectorElement, layers.ClusterElementBuilder)

%{
#include "layers/ClusteredVectorLayer.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "datasources/LocalVectorDataSource.i"
%import "layers/VectorLayer.i"
%import "layers/ClusterElementBuilder.i"

!polymorphic_shared_ptr(massif::ClusteredVectorLayer, layers.ClusteredVectorLayer)

%attribute(massif::ClusteredVectorLayer, float, MinimumClusterDistance, getMinimumClusterDistance, setMinimumClusterDistance)
%attribute(massif::ClusteredVectorLayer, float, MaximumClusterZoom, getMaximumClusterZoom, setMaximumClusterZoom)
%attribute(massif::ClusteredVectorLayer, bool, AnimatedClusters, isAnimatedClusters, setAnimatedClusters)
!attributestring_polymorphic(massif::ClusteredVectorLayer, layers.ClusterElementBuilder, ClusterElementBuilder, getClusterElementBuilder)
%std_exceptions(massif::ClusteredVectorLayer::ClusteredVectorLayer)

%include "layers/ClusteredVectorLayer.h"

#endif
