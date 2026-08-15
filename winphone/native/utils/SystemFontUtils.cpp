#include "utils/SystemFontUtils.h"
#include "core/BinaryData.h"
#include "utils/Log.h"

#include <utf8.h>

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <wrl/client.h>
#include <dwrite.h>

#include <vt/FontNames.h>

namespace massif {

    namespace {

        Microsoft::WRL::ComPtr<IDWriteFontCollection> getSystemFontCollection() {
            using Microsoft::WRL::ComPtr;

            ComPtr<IDWriteFactory> factory;
            HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.GetAddressOf()));
            if (FAILED(hr)) {
                Log::Error("SystemFontUtils: Failed to create DirectWrite factory");
                return ComPtr<IDWriteFontCollection>();
            }

            ComPtr<IDWriteFontCollection> fontCollection;
            hr = factory->GetSystemFontCollection(fontCollection.GetAddressOf());
            if (FAILED(hr)) {
                Log::Error("SystemFontUtils: Failed to get the system font collection");
                return ComPtr<IDWriteFontCollection>();
            }
            return fontCollection;
        }

        bool findFamily(const Microsoft::WRL::ComPtr<IDWriteFontCollection>& fontCollection, const std::string& name, UINT32& familyIndex) {
            std::wstring wname;
            utf8::utf8to16(name.begin(), name.end(), std::back_inserter(wname));
            BOOL familyExists = FALSE;
            HRESULT hr = fontCollection->FindFamilyName(wname.c_str(), &familyIndex, &familyExists);
            return SUCCEEDED(hr) && familyExists;
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
        if (Microsoft::WRL::ComPtr<IDWriteFontCollection> fontCollection = getSystemFontCollection()) {
            for (const std::string& name : vt::parseFontNames(names)) {
                UINT32 familyIndex = 0;
                if (findFamily(fontCollection, name, familyIndex)) {
                    match.familyName = name;
                    break;
                }
            }
        }
        matchCache[names] = match;
        return match;
    }

    std::shared_ptr<BinaryData> SystemFontUtils::LoadFont(const std::string& name, bool allowFallback) {
        using Microsoft::WRL::ComPtr;

        ComPtr<IDWriteFontCollection> fontCollection = getSystemFontCollection();
        if (!fontCollection) {
            return std::shared_ptr<BinaryData>();
        }

        UINT32 familyIndex = 0;
        bool familyExists = findFamily(fontCollection, name, familyIndex);
        if (!familyExists && allowFallback) {
            // Unresolved name, use the default system font
            familyExists = findFamily(fontCollection, "Segoe UI", familyIndex);
        }
        if (!familyExists) {
            // Not an error without the fallback: the caller is walking a font list and tries the next name
            if (allowFallback) {
                Log::Errorf("SystemFontUtils::LoadFont: No system font for %s", name.c_str());
            }
            return std::shared_ptr<BinaryData>();
        }

        ComPtr<IDWriteFontFamily> fontFamily;
        HRESULT hr = fontCollection->GetFontFamily(familyIndex, fontFamily.GetAddressOf());
        if (FAILED(hr)) {
            Log::Errorf("SystemFontUtils::LoadFont: Failed to get the font family of %s", name.c_str());
            return std::shared_ptr<BinaryData>();
        }

        ComPtr<IDWriteFont> font;
        hr = fontFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, font.GetAddressOf());
        if (FAILED(hr)) {
            Log::Errorf("SystemFontUtils::LoadFont: Failed to get the font of %s", name.c_str());
            return std::shared_ptr<BinaryData>();
        }

        ComPtr<IDWriteFontFace> fontFace;
        hr = font->CreateFontFace(fontFace.GetAddressOf());
        if (FAILED(hr)) {
            Log::Errorf("SystemFontUtils::LoadFont: Failed to create the font face of %s", name.c_str());
            return std::shared_ptr<BinaryData>();
        }

        // Only single-file fonts are supported, this is the case for all the system fonts
        UINT32 fileCount = 1;
        ComPtr<IDWriteFontFile> fontFile;
        hr = fontFace->GetFiles(&fileCount, fontFile.GetAddressOf());
        if (FAILED(hr) || fileCount != 1 || !fontFile) {
            Log::Errorf("SystemFontUtils::LoadFont: Failed to get the font file of %s", name.c_str());
            return std::shared_ptr<BinaryData>();
        }

        const void* referenceKey = nullptr;
        UINT32 referenceKeySize = 0;
        ComPtr<IDWriteFontFileLoader> fontFileLoader;
        ComPtr<IDWriteFontFileStream> fontFileStream;
        hr = fontFile->GetReferenceKey(&referenceKey, &referenceKeySize);
        if (SUCCEEDED(hr)) {
            hr = fontFile->GetLoader(fontFileLoader.GetAddressOf());
        }
        if (SUCCEEDED(hr)) {
            hr = fontFileLoader->CreateStreamFromKey(referenceKey, referenceKeySize, fontFileStream.GetAddressOf());
        }
        if (FAILED(hr)) {
            Log::Errorf("SystemFontUtils::LoadFont: Failed to open the font file of %s", name.c_str());
            return std::shared_ptr<BinaryData>();
        }

        UINT64 fileSize = 0;
        hr = fontFileStream->GetFileSize(&fileSize);
        if (FAILED(hr) || fileSize == 0) {
            Log::Errorf("SystemFontUtils::LoadFont: Failed to get the size of the font file of %s", name.c_str());
            return std::shared_ptr<BinaryData>();
        }

        const void* fragmentStart = nullptr;
        void* fragmentContext = nullptr;
        hr = fontFileStream->ReadFileFragment(&fragmentStart, 0, fileSize, &fragmentContext);
        if (FAILED(hr)) {
            Log::Errorf("SystemFontUtils::LoadFont: Failed to read the font file of %s", name.c_str());
            return std::shared_ptr<BinaryData>();
        }
        const unsigned char* bytes = static_cast<const unsigned char*>(fragmentStart);
        std::vector<unsigned char> data(bytes, bytes + static_cast<std::size_t>(fileSize));
        fontFileStream->ReleaseFileFragment(fragmentContext);

        return std::make_shared<BinaryData>(std::move(data));
    }

    SystemFontUtils::SystemFontUtils() {
    }

}
