/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_SYSTEMFONTUTILS_H_
#define _MASSIF_SYSTEMFONTUTILS_H_

#include <memory>
#include <string>

namespace massif {
    class BinaryData;

    /**
     * Access to the fonts installed on the device. Used by vector tile decoders to resolve
     * font names that are not part of the style asset package.
     */
    class SystemFontUtils {
    public:
        /**
         * A system font matched to a requested name, for the platform text APIs the vector
         * elements are drawn with.
         */
        struct FontMatch {
            /**
             * The name the platform text API accepts, empty when no requested name matched.
             */
            std::string familyName;
            /**
             * The font file, set only when the platform has no name for the matched font (Android).
             */
            std::string fileName;
        };

        /**
         * Loads the system font best matching the given name.
         * Generic names ("Arial", "Helvetica", "sans-serif", "serif", "monospace", ...) are mapped
         * to the closest font of the platform.
         * @param name The requested font name, optionally followed by a style ("Arial Bold").
         * @param allowFallback True to answer a name without any match with the default system
         *                      font, false to fail so that the caller can try the next name.
         * @return The font file data or null if there is no match.
         */
        static std::shared_ptr<BinaryData> LoadFont(const std::string& name, bool allowFallback);

        /**
         * Matches a font list to a font installed on the device.
         * @param names A CSS-like font list, the most preferred name first, entries optionally
         *              tagged with the platform they are for ("android:Roboto, ios:Helvetica Neue").
         * @return The first name of the list that the platform knows, empty if none of them does.
         */
        static FontMatch MatchFont(const std::string& names);

    private:
        SystemFontUtils();
    };

}

#endif
