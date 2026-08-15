/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_IOSUTILS_H_
#define _MASSIF_IOSUTILS_H_

#include <mutex>
#include <string>

namespace massif {

    /**
     * IOSUtils is an internal class of the SDK.
     * It is not intended for public usage.
     */
    class IOSUtils {
    public:
        static void InitializeLog();

    private:
        IOSUtils();
    };

}

#endif
