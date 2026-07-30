/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_SYSTEMFONTUTILS_H_
#define _CARTO_SYSTEMFONTUTILS_H_

#include <memory>
#include <string>

namespace carto {
    class BinaryData;

    /**
     * Access to the fonts installed on the device. Used by vector tile decoders to resolve
     * font names that are not part of the style asset package.
     */
    class SystemFontUtils {
    public:
        /**
         * Loads the system font best matching the given name.
         * Generic names ("Arial", "Helvetica", "sans-serif", "serif", "monospace", ...) are mapped
         * to the closest font of the platform, and a name without any match falls back to the
         * default system font.
         * @param name The requested font name, optionally followed by a style ("Arial Bold").
         * @return The font file data or null if the platform has no usable font at all.
         */
        static std::shared_ptr<BinaryData> LoadFont(const std::string& name);

    private:
        SystemFontUtils();
    };

}

#endif
