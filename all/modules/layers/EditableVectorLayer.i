#ifndef _EDITABLEVECTORLAYER_I
#define _EDITABLEVECTORLAYER_I

%module EditableVectorLayer

#ifdef _MASSIF_EDITABLE_SUPPORT

!proxy_imports(massif::EditableVectorLayer, datasources.VectorDataSource, layers.VectorLayer, layers.VectorEditEventListener, vectorelements.VectorElement)

%{
#include "layers/EditableVectorLayer.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "datasources/VectorDataSource.i"
%import "layers/VectorLayer.i"
%import "layers/VectorEditEventListener.i"

!polymorphic_shared_ptr(massif::EditableVectorLayer, layers.EditableVectorLayer)

%attributestring(massif::EditableVectorLayer, std::shared_ptr<massif::VectorElement>, SelectedVectorElement, getSelectedVectorElement, setSelectedVectorElement)
!attributestring_polymorphic(massif::EditableVectorLayer, layers.VectorEditEventListener, VectorEditEventListener, getVectorEditEventListener, setVectorEditEventListener)
%std_exceptions(massif::EditableVectorLayer::EditableVectorLayer)

%include "layers/EditableVectorLayer.h"

#endif

#endif
