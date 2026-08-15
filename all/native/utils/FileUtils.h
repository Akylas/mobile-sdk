/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_FILEUTILS_H_
#define _MASSIF_FILEUTILS_H_

#include <string>

namespace massif {
    
    class FileUtils {
    public:
        static std::string GetFileName(const std::string& fullPath);

        static std::string GetFilePath(const std::string& fullPath);

        static std::string NormalizePath(const std::string& path);
 
    private:
        FileUtils();
    };

}

#endif
