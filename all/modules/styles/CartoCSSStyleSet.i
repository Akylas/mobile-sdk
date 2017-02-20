#ifndef _CARTOCSSSTYLESET_I
#define _CARTOCSSSTYLESET_I

%module CartoCSSStyleSet

!proxy_imports(carto::CartoCSSStyleSet, utils.AssetPackage)

%{
#include "styles/CartoCSSStyleSet.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_string.i>
%include <std_map.i>
%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "utils/AssetPackage.i"

!shared_ptr(carto::CartoCSSStyleSet, styles.CartoCSSStyleSet)

%attributestring(carto::CartoCSSStyleSet, std::string, CartoCSS, getCartoCSS)
%attributestring(carto::CartoCSSStyleSet, std::shared_ptr<carto::AssetPackage>, AssetPackage, getAssetPackage)
%std_exceptions(carto::CartoCSSStyleSet::CartoCSSStyleSet)
!standard_equals(carto::CartoCSSStyleSet);

%include "styles/CartoCSSStyleSet.h"

!value_template(std::map<std::string, std::shared_ptr<carto::CartoCSSStyleSet> >, styles.StringCartoCSSStyleSetMap);

#endif
