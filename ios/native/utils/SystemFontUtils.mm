#include "utils/SystemFontUtils.h"
#include "core/BinaryData.h"
#include "utils/CFUniquePtr.h"
#include "utils/Log.h"

#import <Foundation/Foundation.h>
#import <CoreText/CoreText.h>

namespace carto {

    std::shared_ptr<BinaryData> SystemFontUtils::LoadFont(const std::string& name) {
        NSString* nsName = [NSString stringWithUTF8String:name.c_str()];

        // CoreText substitutes the default system font if the name does not match any installed font
        CFUniquePtr<CTFontRef> font(CTFontCreateWithName((__bridge CFStringRef)nsName, 12.0f, NULL));
        if (!font) {
            Log::Errorf("SystemFontUtils::LoadFont: No system font for %s", name.c_str());
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
