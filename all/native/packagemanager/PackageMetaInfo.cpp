#ifdef _MASSIF_PACKAGEMANAGER_SUPPORT

#include "PackageMetaInfo.h"

namespace massif {

    PackageMetaInfo::PackageMetaInfo(const Variant& var) :
        _variant(var)
    {
    }

    const Variant& PackageMetaInfo::getVariant() const {
        return _variant;
    }

}

#endif
