#include "Variant.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sstream>

namespace routing {

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

Variant::Variant() { _data.type = Data::Type::Null; }

Variant::Variant(bool boolVal) {
    _data.type = Data::Type::Bool;
    _data.boolVal = boolVal;
}

Variant::Variant(long long longVal) {
    _data.type = Data::Type::Integer;
    _data.intVal = static_cast<int64_t>(longVal);
}

Variant::Variant(double doubleVal) {
    _data.type = Data::Type::Double;
    _data.dblVal = doubleVal;
}

Variant::Variant(const char* str) {
    _data.type   = Data::Type::String;
    _data.strVal = str ? str : "";
}

Variant::Variant(const std::string& string) {
    _data.type   = Data::Type::String;
    _data.strVal = string;
}

Variant::Variant(const std::vector<Variant>& array) {
    _data.type   = Data::Type::Array;
    _data.arrVal = std::make_shared<std::vector<Variant>>(array);
}

Variant::Variant(const std::map<std::string, Variant>& object) {
    _data.type   = Data::Type::Object;
    _data.objVal = std::make_shared<std::map<std::string, Variant>>(object);
}

// ---------------------------------------------------------------------------
// Getters
// ---------------------------------------------------------------------------

VariantType::VariantType Variant::getType() const {
    switch (_data.type) {
    case Data::Type::String:  return VariantType::VARIANT_TYPE_STRING;
    case Data::Type::Bool:    return VariantType::VARIANT_TYPE_BOOL;
    case Data::Type::Integer: return VariantType::VARIANT_TYPE_INTEGER;
    case Data::Type::Double:  return VariantType::VARIANT_TYPE_DOUBLE;
    case Data::Type::Array:   return VariantType::VARIANT_TYPE_ARRAY;
    case Data::Type::Object:  return VariantType::VARIANT_TYPE_OBJECT;
    default:                  return VariantType::VARIANT_TYPE_NULL;
    }
}

std::string Variant::getString() const {
    return (_data.type == Data::Type::String) ? _data.strVal : std::string();
}

bool Variant::getBool() const {
    return (_data.type == Data::Type::Bool) ? _data.boolVal : false;
}

long long Variant::getLong() const {
    return (_data.type == Data::Type::Integer) ? static_cast<long long>(_data.intVal) : 0LL;
}

double Variant::getDouble() const {
    return (_data.type == Data::Type::Double) ? _data.dblVal : 0.0;
}

int Variant::getArraySize() const {
    if (_data.type == Data::Type::Array && _data.arrVal) {
        return static_cast<int>(_data.arrVal->size());
    }
    return 0;
}

Variant Variant::getArrayElement(int idx) const {
    if (_data.type == Data::Type::Array && _data.arrVal) {
        const auto& arr = *_data.arrVal;
        if (idx >= 0 && idx < static_cast<int>(arr.size()))
            return arr[static_cast<std::size_t>(idx)];
    }
    return Variant();
}

std::vector<std::string> Variant::getObjectKeys() const {
    std::vector<std::string> keys;
    if (_data.type == Data::Type::Object && _data.objVal) {
        for (const auto& kv : *_data.objVal)
            keys.push_back(kv.first);
    }
    return keys;
}

bool Variant::containsObjectKey(const std::string& key) const {
    if (_data.type == Data::Type::Object && _data.objVal)
        return _data.objVal->count(key) > 0;
    return false;
}

Variant Variant::getObjectElement(const std::string& key) const {
    if (_data.type == Data::Type::Object && _data.objVal) {
        auto it = _data.objVal->find(key);
        if (it != _data.objVal->end())
            return it->second;
    }
    return Variant();
}

void Variant::setObjectElement(const std::string& key, const Variant& value) {
    if (_data.type != Data::Type::Object || !_data.objVal) {
        _data.type   = Data::Type::Object;
        _data.objVal = std::make_shared<std::map<std::string, Variant>>();
    }
    (*_data.objVal)[key] = value;
}

// ---------------------------------------------------------------------------
// Comparison / hash
// ---------------------------------------------------------------------------

bool Variant::operator ==(const Variant& var) const {
    return toJSON() == var.toJSON();
}

bool Variant::operator !=(const Variant& var) const {
    return !(*this == var);
}

int Variant::hash() const {
    return static_cast<int>(std::hash<std::string>()(toJSON()));
}

// ---------------------------------------------------------------------------
// JSON serialization
// ---------------------------------------------------------------------------

std::string Variant::serializeJSON(const Variant& v) {
    switch (v._data.type) {
    case Data::Type::Null:
        return "null";

    case Data::Type::Bool:
        return v._data.boolVal ? "true" : "false";

    case Data::Type::Integer:
        return std::to_string(v._data.intVal);

    case Data::Type::Double: {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.17g", v._data.dblVal);
        return buf;
    }

    case Data::Type::String: {
        std::string out;
        out.reserve(v._data.strVal.size() + 2);
        out += '"';
        for (unsigned char c : v._data.strVal) {
            if      (c == '"')  { out += '\\'; out += '"'; }
            else if (c == '\\') { out += '\\'; out += '\\'; }
            else if (c == '\n') { out += '\\'; out += 'n'; }
            else if (c == '\r') { out += '\\'; out += 'r'; }
            else if (c == '\t') { out += '\\'; out += 't'; }
            else if (c < 0x20)  {
                char esc[8];
                std::snprintf(esc, sizeof(esc), "\\u%04x", static_cast<unsigned>(c));
                out += esc;
            }
            else { out += static_cast<char>(c); }
        }
        out += '"';
        return out;
    }

    case Data::Type::Array: {
        std::string out = "[";
        if (v._data.arrVal) {
            bool first = true;
            for (const auto& elem : *v._data.arrVal) {
                if (!first) out += ',';
                out += serializeJSON(elem);
                first = false;
            }
        }
        return out + ']';
    }

    case Data::Type::Object: {
        std::string out = "{";
        if (v._data.objVal) {
            bool first = true;
            for (const auto& kv : *v._data.objVal) {
                if (!first) out += ',';
                // Serialize key (a plain string)
                out += serializeJSON(Variant(kv.first));
                out += ':';
                out += serializeJSON(kv.second);
                first = false;
            }
        }
        return out + '}';
    }
    }
    return "null";
}

std::string Variant::toString() const { return serializeJSON(*this); }
std::string Variant::toJSON()   const { return serializeJSON(*this); }

// ---------------------------------------------------------------------------
// JSON parsing — minimal recursive-descent parser
// ---------------------------------------------------------------------------

static void skipWS(const char*& p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        ++p;
}

static std::string parseStringRaw(const char*& p, const char* end) {
    // p is pointing at the opening '"'
    ++p; // skip '"'
    std::string out;
    while (p < end && *p != '"') {
        if (*p == '\\') {
            ++p;
            if (p >= end) throw std::runtime_error("Unexpected end in JSON string escape");
            switch (*p) {
            case '"':  out += '"';  break;
            case '\\': out += '\\'; break;
            case '/':  out += '/';  break;
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            case 't':  out += '\t'; break;
            case 'b':  out += '\b'; break;
            case 'f':  out += '\f'; break;
            case 'u': {
                // Consume 4 hex digits; emit as UTF-8 for BMP codepoints
                if (p + 4 >= end) throw std::runtime_error("Incomplete \\u escape");
                unsigned codepoint = 0;
                for (int i = 1; i <= 4; ++i) {
                    char h = p[i];
                    if (h >= '0' && h <= '9') codepoint = codepoint * 16 + (unsigned)(h - '0');
                    else if (h >= 'a' && h <= 'f') codepoint = codepoint * 16 + (unsigned)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') codepoint = codepoint * 16 + (unsigned)(h - 'A' + 10);
                    else throw std::runtime_error("Invalid hex in \\u escape");
                }
                p += 4;
                // Encode as UTF-8
                if (codepoint < 0x80) {
                    out += static_cast<char>(codepoint);
                } else if (codepoint < 0x800) {
                    out += static_cast<char>(0xC0 | (codepoint >> 6));
                    out += static_cast<char>(0x80 | (codepoint & 0x3F));
                } else {
                    out += static_cast<char>(0xE0 | (codepoint >> 12));
                    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
                break;
            }
            default: out += *p; break;
            }
            ++p;
        } else {
            out += *p++;
        }
    }
    if (p < end) ++p; // skip closing '"'
    return out;
}

Variant Variant::parseFromPtr(const char*& p, const char* end) {
    skipWS(p, end);
    if (p >= end) throw std::runtime_error("Unexpected end of JSON");

    char c = *p;

    // String
    if (c == '"') {
        return Variant(parseStringRaw(p, end));
    }

    // Array
    if (c == '[') {
        ++p;
        std::vector<Variant> arr;
        skipWS(p, end);
        if (p < end && *p == ']') { ++p; return Variant(arr); }
        while (true) {
            arr.push_back(parseFromPtr(p, end));
            skipWS(p, end);
            if (p >= end) break;
            if (*p == ']') { ++p; break; }
            if (*p == ',') ++p;
        }
        return Variant(arr);
    }

    // Object
    if (c == '{') {
        ++p;
        std::map<std::string, Variant> obj;
        skipWS(p, end);
        if (p < end && *p == '}') { ++p; return Variant(obj); }
        while (true) {
            skipWS(p, end);
            if (p >= end || *p != '"') throw std::runtime_error("Expected string key in JSON object");
            std::string key = parseStringRaw(p, end);
            skipWS(p, end);
            if (p >= end || *p != ':') throw std::runtime_error("Expected ':' in JSON object");
            ++p;
            obj[key] = parseFromPtr(p, end);
            skipWS(p, end);
            if (p >= end || *p == '}') { if (p < end) ++p; break; }
            if (*p == ',') ++p;
        }
        return Variant(obj);
    }

    // true / false / null
    auto expect = [&](const char* tok, std::size_t len) {
        if (static_cast<std::size_t>(end - p) < len ||
            std::strncmp(p, tok, len) != 0)
            throw std::runtime_error(std::string("Expected '") + tok + "'");
        p += len;
    };

    if (c == 't') { expect("true",  4); return Variant(true);  }
    if (c == 'f') { expect("false", 5); return Variant(false); }
    if (c == 'n') { expect("null",  4); return Variant();      }

    // Number
    if (c == '-' || (c >= '0' && c <= '9')) {
        const char* start = p;
        bool isFloat = false;
        if (*p == '-') ++p;
        while (p < end && *p >= '0' && *p <= '9') ++p;
        if (p < end && *p == '.') {
            isFloat = true;
            ++p;
            while (p < end && *p >= '0' && *p <= '9') ++p;
        }
        if (p < end && (*p == 'e' || *p == 'E')) {
            isFloat = true;
            ++p;
            if (p < end && (*p == '+' || *p == '-')) ++p;
            while (p < end && *p >= '0' && *p <= '9') ++p;
        }
        std::string numStr(start, p);
        if (isFloat) return Variant(std::stod(numStr));
        return Variant(static_cast<long long>(std::stoll(numStr)));
    }

    throw std::runtime_error(std::string("Unexpected JSON character: ") + c);
}

Variant Variant::FromString(const std::string& str) {
    const char* p   = str.data();
    const char* end = p + str.size();
    try {
        Variant v = parseFromPtr(p, end);
        skipWS(p, end);
        if (p != end) throw std::runtime_error("Trailing characters in JSON");
        return v;
    } catch (const std::exception& ex) {
        throw std::runtime_error(std::string("Variant parsing failed: ") + ex.what() + ": " + str);
    }
}

Variant Variant::FromJSON(const std::string& str) {
    return FromString(str);
}

} // namespace routing
