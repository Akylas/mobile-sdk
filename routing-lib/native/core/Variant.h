#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>

#ifndef PICOJSON_USE_INT64
#  define PICOJSON_USE_INT64
#endif
#include <picojson/picojson.h>

namespace routing {

    namespace VariantType {
        enum VariantType {
            VARIANT_TYPE_NULL,
            VARIANT_TYPE_STRING,
            VARIANT_TYPE_BOOL,
            VARIANT_TYPE_INTEGER,
            VARIANT_TYPE_DOUBLE,
            VARIANT_TYPE_ARRAY,
            VARIANT_TYPE_OBJECT
        };
    }

    /**
     * JSON value wrapper backed by picojson. No external (boost) dependencies.
     */
    class Variant {
    public:
        Variant();
        explicit Variant(bool boolVal);
        explicit Variant(long long longVal);
        explicit Variant(double doubleVal);
        explicit Variant(const char* str);
        explicit Variant(const std::string& string);
        explicit Variant(const std::vector<Variant>& array);
        explicit Variant(const std::map<std::string, Variant>& object);

        VariantType::VariantType getType() const;

        std::string getString() const;
        bool getBool() const;
        long long getLong() const;
        double getDouble() const;

        int getArraySize() const;
        Variant getArrayElement(int idx) const;

        std::vector<std::string> getObjectKeys() const;
        bool containsObjectKey(const std::string& key) const;
        Variant getObjectElement(const std::string& key) const;

        bool operator ==(const Variant& var) const;
        bool operator !=(const Variant& var) const;

        int hash() const;
        std::string toString() const;

        const picojson::value& toPicoJSON() const;

        static Variant FromString(const std::string& str);
        static Variant FromPicoJSON(picojson::value val);

    private:
        picojson::value _value;
    };

} // namespace routing
