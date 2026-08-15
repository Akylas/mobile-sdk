/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_ROUTINGPACKAGEHANDLER_H_
#define _MASSIF_ROUTINGPACKAGEHANDLER_H_

#if defined(_MASSIF_ROUTING_SUPPORT) && defined(_MASSIF_PACKAGEMANAGER_SUPPORT)

#include "packagemanager/handlers/PackageHandler.h"

#include <fstream>

namespace massif {
    
    class RoutingPackageHandler : public PackageHandler {
    public:
        explicit RoutingPackageHandler(const std::string& fileName);
        virtual ~RoutingPackageHandler();

        std::shared_ptr<std::ifstream> getGraphFile();

        virtual void onImportPackage();
        virtual void onDeletePackage();

        virtual std::shared_ptr<PackageTileMask> calculateTileMask() const;

    private:
        std::shared_ptr<std::ifstream> _graphFile;
    };
    
}

#endif

#endif
