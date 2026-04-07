#include "Variant.h"

#include <stdexcept>

namespace routing {

    Variant::Variant() : _value() {}
    Variant::Variant(bool boolVal) : _value(boolVal) {}
    Variant::Variant(long long longVal) : _value(static_cast<std::int64_t>(longVal)) {}
    Variant::Variant(double doubleVal) : _value(doubleVal) {}
    Variant::Variant(const char* str) : _value(std::string(str)) {}
    Variant::Variant(const std::string& string) : _value(string) {}

    Variant::Variant(const std::vector<Variant>& array) : _value() {
        picojson::value::array valArr;
        for (const auto& elem : array) valArr.push_back(elem.toPicoJSON());
        _value = picojson::value(valArr);
    }

    Variant::Variant(const std::map<std::string, Variant>& object) : _value() {
        picojson::value::object valObj;
        for (const auto& kv : object) valObj[kv.first] = kv.second.toPicoJSON();
        _value = picojson::value(valObj);
    }

    VariantType::VariantType Variant::getType() const {
        const picojson::value& val = toPicoJSON();
        if (val.is<std::string>())                return VariantType::VARIANT_TYPE_STRING;
        if (val.is<bool>())                       return VariantType::VARIANT_TYPE_BOOL;
        if (val.is<std::int64_t>())               return VariantType::VARIANT_TYPE_INTEGER;
        if (val.is<double>())                     return VariantType::VARIANT_TYPE_DOUBLE;
        if (val.is<picojson::value::array>())     return VariantType::VARIANT_TYPE_ARRAY;
        if (val.is<picojson::value::object>())    return VariantType::VARIANT_TYPE_OBJECT;
        return VariantType::VARIANT_TYPE_NULL;
    }

    std::string Variant::getString() const { return toPicoJSON().to_str(); }

    bool Variant::getBool() const {
        const picojson::value& val = toPicoJSON();
        return val.is<bool>() ? val.get<bool>() : false;
    }

    long long Variant::getLong() const {
        const picojson::value& val = toPicoJSON();
        return val.is<std::int64_t>() ? val.get<std::int64_t>() : 0LL;
    }

    double Variant::getDouble() const {
        const picojson::value& val = toPicoJSON();
        return val.is<double>() ? val.get<double>() : 0.0;
    }

    int Variant::getArraySize() const {
        const picojson::value& val = toPicoJSON();
        return val.is<picojson::value::array>() ? static_cast<int>(val.get<picojson::value::array>().size()) : 0;
    }

    Variant Variant::getArrayElement(int idx) const {
        const picojson::value& val = toPicoJSON();
        if (val.is<picojson::value::array>()) {
            const auto& arr = val.get<picojson::value::array>();
            if (idx >= 0 && idx < static_cast<int>(arr.size()))
                return FromPicoJSON(arr[idx]);
        }
        return Variant();
    }

    std::vector<std::string> Variant::getObjectKeys() const {
        std::vector<std::string> keys;
        const picojson::value& val = toPicoJSON();
        if (val.is<picojson::value::object>()) {
            for (const auto& kv : val.get<picojson::value::object>())
                keys.push_back(kv.first);
        }
        return keys;
    }

    bool Variant::containsObjectKey(const std::string& key) const {
        const picojson::value& val = toPicoJSON();
        return val.is<picojson::value::object>() && val.contains(key);
    }

    Variant Variant::getObjectElement(const std::string& key) const {
        const picojson::value& val = toPicoJSON();
        if (val.is<picojson::value::object>()) {
            const auto& obj = val.get<picojson::value::object>();
            auto it = obj.find(key);
            if (it != obj.end()) return FromPicoJSON(it->second);
        }
        return Variant();
    }

    bool Variant::operator ==(const Variant& var) const { return toPicoJSON() == var.toPicoJSON(); }
    bool Variant::operator !=(const Variant& var) const { return !(*this == var); }

    int Variant::hash() const { return static_cast<int>(std::hash<std::string>()(toString())); }
    std::string Variant::toString() const { return toPicoJSON().serialize(); }
    const picojson::value& Variant::toPicoJSON() const { return _value; }

    Variant Variant::FromString(const std::string& str) {
        picojson::value val;
        std::string err = picojson::parse(val, str);
        if (!err.empty()) {
            throw std::runtime_error("Variant parsing failed: " + err + ": " + str);
        }
        Variant var;
        var._value = std::move(val);
        return var;
    }

    Variant Variant::FromPicoJSON(picojson::value val) {
        Variant var;
        var._value = std::move(val);
        return var;
    }

} // namespace routing
