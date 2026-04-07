#include "RouteMatchingRequest.h"
#include "../exceptions/Exceptions.h"
#include "../utils/StringUtils.h"

#include <iomanip>
#include <sstream>

namespace routing {

static Variant getNestedVariant(const Variant& root,
                                 const std::vector<std::string>& keys) {
    Variant cur = root;
    for (const auto& k : keys) {
        if (cur.getType() != VariantType::VARIANT_TYPE_OBJECT) return Variant();
        cur = cur.getObjectElement(k);
    }
    return cur;
}

static Variant setNestedVariant(Variant root,
                                 const std::vector<std::string>& keys,
                                 int idx,
                                 const Variant& value) {
    if (idx == static_cast<int>(keys.size()) - 1) {
        root.setObjectElement(keys[static_cast<std::size_t>(idx)], value);
        return root;
    }
    const std::string& key = keys[static_cast<std::size_t>(idx)];
    Variant sub = root.getObjectElement(key);
    if (sub.getType() != VariantType::VARIANT_TYPE_OBJECT)
        sub = Variant(std::map<std::string, Variant>{});
    root.setObjectElement(key, setNestedVariant(std::move(sub), keys, idx + 1, value));
    return root;
}

// ---------------------------------------------------------------------------
// RouteMatchingRequest implementation
// ---------------------------------------------------------------------------

RouteMatchingRequest::RouteMatchingRequest(const std::shared_ptr<Projection>& projection,
                                           const std::vector<MapPos>& points,
                                           float accuracy) :
    _projection(projection),
    _points(points),
    _accuracy(accuracy)
{
    if (!projection) throw NullArgumentException("Null projection");
}

RouteMatchingRequest::~RouteMatchingRequest() {}

const std::shared_ptr<Projection>& RouteMatchingRequest::getProjection() const { return _projection; }
const std::vector<MapPos>&         RouteMatchingRequest::getPoints()     const { return _points; }
float                              RouteMatchingRequest::getAccuracy()   const { return _accuracy; }

Variant RouteMatchingRequest::getPointParameters(int index) const {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _pointParams.find(index);
    return it == _pointParams.end() ? Variant() : it->second;
}

Variant RouteMatchingRequest::getPointParameter(int index, const std::string& param) const {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _pointParams.find(index);
    if (it == _pointParams.end()) return Variant();
    return getNestedVariant(it->second, splitKeys(param, '.'));
}

void RouteMatchingRequest::setPointParameter(int index, const std::string& param,
                                              const Variant& value) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto keys = splitKeys(param, '.');
    _pointParams[index] = setNestedVariant(
        _pointParams.count(index) ? _pointParams.at(index) : Variant(std::map<std::string,Variant>{}),
        keys, 0, value);
}

Variant RouteMatchingRequest::getCustomParameters() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _customParams;
}

Variant RouteMatchingRequest::getCustomParameter(const std::string& param) const {
    std::lock_guard<std::mutex> lock(_mutex);
    return getNestedVariant(_customParams, splitKeys(param, '.'));
}

void RouteMatchingRequest::setCustomParameter(const std::string& param, const Variant& value) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto keys = splitKeys(param, '.');
    _customParams = setNestedVariant(std::move(_customParams), keys, 0, value);
}

std::string RouteMatchingRequest::toString() const {
    std::lock_guard<std::mutex> lock(_mutex);
    std::stringstream ss;
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
