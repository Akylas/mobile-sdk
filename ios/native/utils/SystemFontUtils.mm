#include "utils/SystemFontUtils.h"
#include "core/BinaryData.h"
#include "utils/CFUniquePtr.h"
#include "utils/Log.h"

#include <cctype>
#include <map>
#include <mutex>

#include <vt/FontNames.h>

#import <Foundation/Foundation.h>
#import <CoreText/CoreText.h>

namespace massif {

    namespace {

        std::string normalizeFontName(const std::string& name) {
            std::string normalized;
            for (char c : name) {
                if (std::isalnum(static_cast<unsigned char>(c))) {
                    normalized.append(1, static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                }
            }
            return normalized;
        }

        std::string toStdString(CFStringRef stringRef) {
            return stringRef ? std::string([(__bridge NSString*)stringRef UTF8String]) : std::string();
        }

        // CoreText substitutes the default system font instead of failing, so what came back has to
        // be compared with what was asked for. The family is enough for a style of it
        // ('HelveticaNeue-Light' is the 'Helvetica Neue' family).
        bool matchesRequestedName(CTFontRef font, const std::string& name) {
            std::string normalizedName = normalizeFontName(name);
            CFUniquePtr<CFStringRef> postScriptName(CTFontCopyPostScriptName(font));
            if (normalizeFontName(toStdString(postScriptName)) == normalizedName) {
                return true;
            }
            CFUniquePtr<CFStringRef> familyName(CTFontCopyFamilyName(font));
            std::string normalizedFamily = normalizeFontName(toStdString(familyName));
            return !normalizedFamily.empty() && normalizedName.compare(0, normalizedFamily.size(), normalizedFamily) == 0;
        }

        CFUniquePtr<CTFontRef> createFont(const std::string& name) {
            NSString* nsName = [NSString stringWithUTF8String:name.c_str()];
            return CFUniquePtr<CTFontRef>(CTFontCreateWithName((__bridge CFStringRef)nsName, 12.0f, NULL));
        }

    }

    SystemFontUtils::FontMatch SystemFontUtils::MatchFont(const std::string& names) {
        static std::mutex mutex;
        static std::map<std::string, FontMatch> matchCache;

        std::lock_guard<std::mutex> lock(mutex);
        auto it = matchCache.find(names);
        if (it != matchCache.end()) {
            return it->second;
        }

        FontMatch match;
        for (const std::string& name : vt::parseFontNames(names)) {
            CFUniquePtr<CTFontRef> font = createFont(name);
            if (font && matchesRequestedName(font, name)) {
                match.familyName = name;
                break;
            }
        }
        matchCache[names] = match;
        return match;
    }

    std::shared_ptr<BinaryData> SystemFontUtils::LoadFont(const std::string& name, bool allowFallback) {
        CFUniquePtr<CTFontRef> font = createFont(name);
        if (!font || (!allowFallback && !matchesRequestedName(font, name))) {
            // Not an error without the fallback: the caller is walking a font list and tries the next name
            if (allowFallback) {
                Log::Errorf("SystemFontUtils::LoadFont: No system font for %s", name.c_str());
            }
            return std::shared_ptr<BinaryData>();
        }

        // Note: for a font collection (.ttc) this gives the file of the whole collection, the first face is used
        CFUniquePtr<CFURLRef> fontURL(static_cast<CFURLRef>(CTFontCopyAttribute(font, kCTFontURLAttribute)));
        if (!fontURL) {
            Log::Errorf("SystemFontUtils::LoadFont: Failed to resolve the file of %s", name.c_str());
            return std::shared_ptr<BinaryData>();
        }
        NSString* filePath = [(__bridge NSURL*)fontURL.get() path];

        NSData* fileData = [NSData dataWithContentsOfFile:filePath];
        if (!fileData) {
            Log::Errorf("SystemFontUtils::LoadFont: Failed to read %s", [filePath UTF8String]);
            return std::shared_ptr<BinaryData>();
        }

        std::vector<unsigned char> data;
        const unsigned char* bytes = static_cast<const unsigned char*>([fileData bytes]);
        data.assign(bytes, bytes + [fileData length]);
        Log::Infof("SystemFontUtils::LoadFont: Using %s for %s", [filePath UTF8String], name.c_str());
        return std::make_shared<BinaryData>(std::move(data));
    }

    SystemFontUtils::SystemFontUtils() {
    }

}
