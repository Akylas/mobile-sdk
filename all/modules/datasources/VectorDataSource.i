#ifndef _VECTORDATASOURCE_I
#define _VECTORDATASOURCE_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module(directors="1") VectorDataSource

!proxy_imports(massif::VectorDataSource, core.MapBounds, datasources.components.VectorData, projections.Projection, renderers.components.CullState, graphics.ViewState)

%{
#include "datasources/VectorDataSource.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapBounds.i"
%import "datasources/components/VectorData.i"
%import "projections/Projection.i"
%import "renderers/components/CullState.i"
%import "graphics/ViewState.i"

!polymorphic_shared_ptr(massif::VectorDataSource, datasources.VectorDataSource)

%feature("director") massif::VectorDataSource;

%attributeval(massif::VectorDataSource, massif::MapBounds, DataExtent, getDataExtent)
!attributestring_polymorphic(massif::VectorDataSource, projections.Projection, Projection, getProjection)
%std_exceptions(massif::VectorDataSource::VectorDataSource)
%ignore massif::VectorDataSource::OnChangeListener;
%ignore massif::VectorDataSource::registerOnChangeListener;
%ignore massif::VectorDataSource::unregisterOnChangeListener;
%ignore massif::VectorDataSource::getElementDataSource;

%feature("nodirector") massif::VectorDataSource::notifyElementAdded;
%feature("nodirector") massif::VectorDataSource::notifyElementChanged;
%feature("nodirector") massif::VectorDataSource::notifyElementRemoved;
%feature("nodirector") massif::VectorDataSource::notifyElementsAdded;
%feature("nodirector") massif::VectorDataSource::notifyElementsRemoved;
%feature("nodirector") massif::VectorDataSource::attachElement;
%feature("nodirector") massif::VectorDataSource::detachElement;

%include "datasources/VectorDataSource.h"

#endif
