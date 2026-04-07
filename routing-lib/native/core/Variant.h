#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
     * Polymorphic JSON value. Self-contained — no external JSON-library dependency.
     *
     * Storage uses a tagged-union struct; array/object payloads are shared_ptr so
     * copying is cheap (shared ownership, copy-on-write semantics not enforced).
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

        /** Returns the unquoted string value. For non-string types returns "". */
        std::string getString() const;
        bool getBool() const;
        long long getLong() const;
        double getDouble() const;

        int getArraySize() const;
        Variant getArrayElement(int idx) const;

        std::vector<std::string> getObjectKeys() const;
        bool containsObjectKey(const std::string& key) const;
        Variant getObjectElement(const std::string& key) const;
        /** Set (or insert) a key in this object variant. Turns null/non-object into object. */
        void setObjectElement(const std::string& key, const Variant& value);

        bool operator ==(const Variant& var) const;
        bool operator !=(const Variant& var) const;

        int hash() const;

        /** Returns a JSON-serialized string of this value. */
        std::string toString() const;
        std::string toJSON() const;

        /** Parse a JSON string. Throws std::runtime_error on invalid JSON. */
        static Variant FromString(const std::string& str);
        /** Alias for FromString(). */
        static Variant FromJSON(const std::string& str);

    private:
        struct Data {
            enum class Type : uint8_t { Null, Bool, Integer, Double, String, Array, Object };
            Type    type    = Type::Null;
            bool    boolVal = false;
            int64_t intVal  = 0;
            double  dblVal  = 0.0;
            std::string strVal;
            std::shared_ptr<std::vector<Variant>> arrVal;
            std::shared_ptr<std::map<std::string, Variant>> objVal;
        };

        Data _data;

        static Variant parseFromPtr(const char*& p, const char* end);
        static std::string serializeJSON(const Variant& v);
    };

} // namespace routing
