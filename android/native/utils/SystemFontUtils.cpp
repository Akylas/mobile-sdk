#include "utils/SystemFontUtils.h"
#include "core/BinaryData.h"
#include "utils/Log.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <mutex>
#include <vector>

#include <dirent.h>

namespace massif {

    namespace {

        const char* const FONT_DIRECTORIES[] = { "/system/fonts", "/product/fonts", "/system/font", "/data/fonts", nullptr };

        // Generic/foreign font names mapped to the Android font families, in preference order
        const std::pair<const char*, const char*> FONT_ALIASES[] = {
            { "arial",          "roboto notosans droidsans" },
            { "helvetica",      "roboto notosans droidsans" },
            { "helveticaneue",  "roboto notosans droidsans" },
            { "verdana",        "roboto notosans droidsans" },
            { "tahoma",         "roboto notosans droidsans" },
            { "segoeui",        "roboto notosans droidsans" },
            { "sansserif",      "roboto notosans droidsans" },
            { "sans",           "roboto notosans droidsans" },
            { "timesnewroman",  "notoserif droidserif tinos" },
            { "times",          "notoserif droidserif tinos" },
            { "georgia",        "notoserif droidserif tinos" },
            { "serif",          "notoserif droidserif tinos" },
            { "couriernew",     "droidsansmono notosansmono cousine" },
            { "courier",        "droidsansmono notosansmono cousine" },
            { "consolas",       "droidsansmono notosansmono cousine" },
            { "monospace",      "droidsansmono notosansmono cousine" },
            { "mono",           "droidsansmono notosansmono cousine" },
            { nullptr, nullptr }
        };

        const char* const STYLE_SUFFIXES[] = { "bolditalic", "boldoblique", "bold", "italic", "oblique", "regular", "book", nullptr };

        const char* const DEFAULT_FONTS[] = { "robotoregular", "notosansregular", "droidsans", "robotobold", nullptr };

        std::string normalizeFontName(const std::string& name) {
            std::string normalized;
            for (char c : name) {
                if (std::isalnum(static_cast<unsigned char>(c))) {
                    normalized.append(1, static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                }
            }
            return normalized;
        }

        // Maps normalized font names to font files. Built once, the set of installed fonts is fixed.
        const std::map<std::string, std::string>& getSystemFontMap() {
            static std::mutex mutex;
            static std::map<std::string, std::string> fontMap;
            static bool initialized = false;

            std::lock_guard<std::mutex> lock(mutex);
            if (!initialized) {
                initialized = true;
                for (int i = 0; FONT_DIRECTORIES[i]; i++) {
                    DIR* dir = opendir(FONT_DIRECTORIES[i]);
                    if (!dir) {
                        continue;
                    }
                    while (struct dirent* entry = readdir(dir)) {
                        std::string fileName = entry->d_name;
                        std::size_t extPos = fileName.rfind('.');
                        if (extPos == std::string::npos) {
                            continue;
                        }
                        std::string ext = normalizeFontName(fileName.substr(extPos));
                        if (ext != "ttf" && ext != "otf" && ext != "ttc") {
                            continue;
                        }
                        fontMap.emplace(normalizeFontName(fileName.substr(0, extPos)), std::string(FONT_DIRECTORIES[i]) + "/" + fileName);
                    }
                    closedir(dir);
                }
                Log::Infof("SystemFontUtils: found %d system fonts", static_cast<int>(fontMap.size()));
            }
            return fontMap;
        }

        std::string findFontFile(const std::map<std::string, std::string>& fontMap, const std::string& normalizedName) {
            if (normalizedName.empty()) {
                return std::string();
            }

            auto it = fontMap.find(normalizedName);
            if (it != fontMap.end()) {
                return it->second;
            }
            it = fontMap.find(normalizedName + "regular");
            if (it != fontMap.end()) {
                return it->second;
            }

            // Accept a variant of the family ('roboto' -> 'robotomedium'), preferring the shortest name
            for (auto it2 = fontMap.lower_bound(normalizedName); it2 != fontMap.end(); it2++) {
                if (it2->first.compare(0, normalizedName.size(), normalizedName) != 0) {
                    break;
                }
                return it2->second;
            }
            return std::string();
        }

        std::string resolveFontFile(const std::string& name) {
            const std::map<std::string, std::string>& fontMap = getSystemFontMap();
            std::string normalizedName = normalizeFontName(name);

            std::string fileName = findFontFile(fontMap, normalizedName);
            if (!fileName.empty()) {
                return fileName;
            }

            // Split the trailing style ('arialbold' -> 'arial' + 'bold') and try the aliases of the family
            std::string family = normalizedName;
            std::string style;
            for (int i = 0; STYLE_SUFFIXES[i]; i++) {
                std::string suffix = STYLE_SUFFIXES[i];
                if (normalizedName.size() > suffix.size() && normalizedName.compare(normalizedName.size() - suffix.size(), suffix.size(), suffix) == 0) {
                    family = normalizedName.substr(0, normalizedName.size() - suffix.size());
                    style = suffix;
                    break;
                }
            }
            for (int i = 0; FONT_ALIASES[i].first; i++) {
                if (family != FONT_ALIASES[i].first) {
                    continue;
                }
                std::string candidates = FONT_ALIASES[i].second;
                for (std::size_t pos = 0; pos < candidates.size(); ) {
                    std::size_t spacePos = candidates.find(' ', pos);
                    std::string candidate = candidates.substr(pos, spacePos == std::string::npos ? std::string::npos : spacePos - pos);
                    pos = (spacePos == std::string::npos ? candidates.size() : spacePos + 1);

                    fileName = findFontFile(fontMap, candidate + style);
                    if (fileName.empty()) {
                        fileName = findFontFile(fontMap, candidate);
                    }
                    if (!fileName.empty()) {
                        return fileName;
                    }
                }
                break;
            }

            // Unresolved name, use the default system font
            for (int i = 0; DEFAULT_FONTS[i]; i++) {
                fileName = findFontFile(fontMap, DEFAULT_FONTS[i]);
                if (!fileName.empty()) {
                    return fileName;
                }
            }
            return fontMap.empty() ? std::string() : fontMap.begin()->second;
        }

    }

    std::shared_ptr<BinaryData> SystemFontUtils::LoadFont(const std::string& name) {
        std::string fileName = resolveFontFile(name);
        if (fileName.empty()) {
            Log::Errorf("SystemFontUtils::LoadFont: No system font for %s", name.c_str());
            return std::shared_ptr<BinaryData>();
        }

        std::ifstream file(fileName.c_str(), std::ios::binary);
        if (!file) {
            Log::Errorf("SystemFontUtils::LoadFont: Failed to open %s", fileName.c_str());
            return std::shared_ptr<BinaryData>();
        }
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (data.empty()) {
            Log::Errorf("SystemFontUtils::LoadFont: Failed to read %s", fileName.c_str());
            return std::shared_ptr<BinaryData>();
        }
        Log::Infof("SystemFontUtils::LoadFont: Using %s for %s", fileName.c_str(), name.c_str());
        return std::make_shared<BinaryData>(std::move(data));
    }

    SystemFontUtils::SystemFontUtils() {
    }

}
