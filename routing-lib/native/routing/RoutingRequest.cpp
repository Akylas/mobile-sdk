#include "RoutingRequest.h"
#include "../exceptions/Exceptions.h"

#include <iomanip>
#include <sstream>
#include <string>

// C++17 helper: split a string by a delimiter character (replaces boost::split)
static std::vector<std::string> splitKeys(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::string current;
    for (char c : s) {
        if (c == delim) {
            result.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    result.push_back(current);
    return result;
}

namespace routing {

    RoutingRequest::RoutingRequest(const std::shared_ptr<Projection>& projection,
                                   const std::vector<MapPos>& points) :
        _projection(projection),
        _points(points),
        _pointParams(),
        _customParams(),
        _mutex()
    {
        if (!projection) throw NullArgumentException("Null projection");
    }

    RoutingRequest::~RoutingRequest() {}

    const std::shared_ptr<Projection>& RoutingRequest::getProjection() const { return _projection; }
    const std::vector<MapPos>& RoutingRequest::getPoints() const { return _points; }

    Variant RoutingRequest::getPointParameters(int index) const {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _pointParams.find(index);
        return it == _pointParams.end() ? Variant() : it->second;
    }

    Variant RoutingRequest::getPointParameter(int index, const std::string& param) const {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _pointParams.find(index);
        if (it == _pointParams.end()) return Variant();
        std::vector<std::string> keys = splitKeys(param, '.');
        picojson::value sub = it->second.toPicoJSON();
        for (const auto& key : keys) {
            if (!sub.is<picojson::object>()) return Variant();
            sub = sub.get(key);
        }
        return Variant::FromPicoJSON(sub);
    }

    void RoutingRequest::setPointParameter(int index, const std::string& param, const Variant& value) {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::string> keys = splitKeys(param, '.');
        picojson::value root = _pointParams[index].toPicoJSON();
        picojson::value* sub = &root;
        for (const auto& key : keys) {
            if (!sub->is<picojson::object>()) sub->set(picojson::object());
            sub = &sub->get<picojson::object>()[key];
        }
        *sub = value.toPicoJSON();
        _pointParams[index] = Variant::FromPicoJSON(root);
    }

    Variant RoutingRequest::getCustomParameters() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _customParams;
    }

    Variant RoutingRequest::getCustomParameter(const std::string& param) const {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::string> keys = splitKeys(param, '.');
        picojson::value sub = _customParams.toPicoJSON();
        for (const auto& key : keys) {
            if (!sub.is<picojson::object>()) return Variant();
            sub = sub.get(key);
        }
        return Variant::FromPicoJSON(sub);
    }

    void RoutingRequest::setCustomParameter(const std::string& param, const Variant& value) {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::string> keys = splitKeys(param, '.');
        picojson::value root = _customParams.toPicoJSON();
        picojson::value* sub = &root;
        for (const auto& key : keys) {
            if (!sub->is<picojson::object>()) sub->set(picojson::object());
            sub = &sub->get<picojson::object>()[key];
        }
        *sub = value.toPicoJSON();
        _customParams = Variant::FromPicoJSON(root);
    }

    std::string RoutingRequest::toString() const {
        std::lock_guard<std::mutex> lock(_mutex);
        std::stringstream ss;
        ss << std::setiosflags(std::ios::fixed);
        ss << "RoutingRequest [points=[";
        for (auto it = _points.begin(); it != _points.end(); ++it) {
            ss << (it == _points.begin() ? "" : ", ") << it->toString();
        }
        ss << "]";
        if (!_pointParams.empty()) {
            ss << ", pointParams=[";
            for (auto it = _pointParams.begin(); it != _pointParams.end(); ++it) {
                ss << (it == _pointParams.begin() ? "" : ", ") << it->first << "=" << it->second.toString();
            }
            ss << "]";
        }
        if (_customParams.getType() != VariantType::VARIANT_TYPE_NULL) {
            ss << ", customParams=" << _customParams.toString();
        }
        ss << "]";
        return ss.str();
    }

} // namespace routing
