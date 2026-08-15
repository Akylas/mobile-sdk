#ifndef _VECTORELEMENTSEARCHSERVICE_I
#define _VECTORELEMENTSEARCHSERVICE_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module(directors="1") VectorElementSearchService

#ifdef _MASSIF_SEARCH_SUPPORT

!proxy_imports(massif::VectorElementSearchService, search.SearchRequest, datasources.VectorDataSource, vectorelements.VectorElement, vectorelements.VectorElementVector, projections.Projection)

%{
#include "search/VectorElementSearchService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "search/SearchRequest.i"
%import "datasources/VectorDataSource.i"
%import "vectorelements/VectorElement.i"
%import "projections/Projection.i"

!polymorphic_shared_ptr(massif::VectorElementSearchService, search.VectorElementSearchService)

%attributestring(massif::VectorElementSearchService, std::shared_ptr<massif::VectorDataSource>, DataSource, getDataSource)
%attribute(massif::VectorElementSearchService, int, MaxResults, getMaxResults, setMaxResults)
%std_exceptions(massif::VectorElementSearchService::VectorElementSearchService)
%std_exceptions(massif::VectorElementSearchService::findElements)

%feature("director") massif::VectorElementSearchService;

%include "search/VectorElementSearchService.h"

#endif

#endif
