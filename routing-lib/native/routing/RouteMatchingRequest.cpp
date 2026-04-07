#include "RouteMatchingRequest.h"
#include "../exceptions/Exceptions.h"

#include <iomanip>
#include <sstream>

static std::vector<std::string> splitKeys(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::string current;
    for (char c : s) {
        if (c == delim) { result.push_back(current); current.clear(); }
        else current += c;
    }
    result.push_back(current);
    return result;
}

namespace routing {

    RouteMatchingRequest::RouteMatchingRequest(const std::shared_ptr<Projection>& projection,
                                               const std::vector<MapPos>& points,
                                               float accuracy) :
        _projection(projection),
        _points(points),
        _accuracy(accuracy),
        _pointParams(),
        _customParams(),
        _mutex()
    {
        if (!projection) throw NullArgumentException("Null projection");
    }

    RouteMatchingRequest::~RouteMatchingRequest() {}

    const std::shared_ptr<Projection>& RouteMatchingRequest::getProjection() const { return _projection; }
    const std::vector<MapPos>& RouteMatchingRequest::getPoints() const { return _points; }
    float RouteMatchingRequest::getAccuracy() const { return _accuracy; }

    Variant RouteMatchingRequest::getPointParameters(int index) const {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _pointParams.find(index);
        return it == _pointParams.end() ? Variant() : it->second;
    }

    Variant RouteMatchingRequest::getPointParameter(int index, const std::string& param) const {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _pointParams.find(index);
        if (it == _pointParams.end()) return Variant();
        auto keys = splitKeys(param, '.');
        picojson::value sub = it->second.toPicoJSON();
        for (const auto& k : keys) {
            if (!sub.is<picojson::object>()) return Variant();
            sub = sub.get(k);
        }
        return Variant::FromPicoJSON(sub);
    }

    void RouteMatchingRequest::setPointParameter(int index, const std::string& param, const Variant& value) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto keys = splitKeys(param, '.');
        picojson::value root = _pointParams[index].toPicoJSON();
        picojson::value* sub = &root;
        for (const auto& k : keys) {
            if (!sub->is<picojson::object>()) sub->set(picojson::object());
            sub = &sub->get<picojson::object>()[k];
        }
        *sub = value.toPicoJSON();
        _pointParams[index] = Variant::FromPicoJSON(root);
    }

    Variant RouteMatchingRequest::getCustomParameters() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _customParams;
    }

    Variant RouteMatchingRequest::getCustomParameter(const std::string& param) const {
        std::lock_guard<std::mutex> lock(_mutex);
        auto keys = splitKeys(param, '.');
        picojson::value sub = _customParams.toPicoJSON();
        for (const auto& k : keys) {
            if (!sub.is<picojson::object>()) return Variant();
            sub = sub.get(k);
        }
        return Variant::FromPicoJSON(sub);
    }

    void RouteMatchingRequest::setCustomParameter(const std::string& param, const Variant& value) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto keys = splitKeys(param, '.');
        picojson::value root = _customParams.toPicoJSON();
        picojson::value* sub = &root;
        for (const auto& k : keys) {
            if (!sub->is<picojson::object>()) sub->set(picojson::object());
            sub = &sub->get<picojson::object>()[k];
        }
        *sub = value.toPicoJSON();
        _customParams = Variant::FromPicoJSON(root);
    }

    std::string RouteMatchingRequest::toString() const {
        std::lock_guard<std::mutex> lock(_mutex);
        std::stringstream ss;
        ss << std::setiosflags(std::ios::fixed);
        ss << "RouteMatchingRequest [points=[";
        for (auto it = _points.begin(); it != _points.end(); ++it)
            ss << (it == _points.begin() ? "" : ", ") << it->toString();
        ss << "], accuracy=" << _accuracy;
        if (_customParams.getType() != VariantType::VARIANT_TYPE_NULL)
            ss << ", customParams=" << _customParams.toString();
        ss << "]";
        return ss.str();
    }

} // namespace routing
