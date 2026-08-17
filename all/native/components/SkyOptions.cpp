#include "SkyOptions.h"

#include <algorithm>

namespace massif {

    SkyOptions::SkyOptions() :
        _enabled(true),
        _skyColorARGB(Color(58, 116, 196, 255).getARGB()),
        _horizonColorARGB(Color(171, 206, 236, 255).getARGB()),
        _groundColorARGB(Color(171, 206, 236, 255).getARGB()),
        _horizonBlend(12.0f),
        _sunDiscEnabled(true),
        _shaderSource(),
        _shaderSourceMutex(),
        _onChangeListeners(),
        _onChangeListenersMutex()
    {
    }

    SkyOptions::~SkyOptions() {
    }

    bool SkyOptions::isEnabled() const {
        return _enabled.load();
    }

    void SkyOptions::setEnabled(bool enabled) {
        if (_enabled.exchange(enabled) != enabled) {
            notifyOptionChanged("Enabled");
        }
    }

    Color SkyOptions::getSkyColor() const {
        return Color(_skyColorARGB.load());
    }

    void SkyOptions::setSkyColor(const Color& color) {
        if (_skyColorARGB.exchange(color.getARGB()) != color.getARGB()) {
            notifyOptionChanged("SkyColor");
        }
    }

    Color SkyOptions::getHorizonColor() const {
        return Color(_horizonColorARGB.load());
    }

    void SkyOptions::setHorizonColor(const Color& color) {
        if (_horizonColorARGB.exchange(color.getARGB()) != color.getARGB()) {
            notifyOptionChanged("HorizonColor");
        }
    }

    Color SkyOptions::getGroundColor() const {
        return Color(_groundColorARGB.load());
    }

    void SkyOptions::setGroundColor(const Color& color) {
        if (_groundColorARGB.exchange(color.getARGB()) != color.getARGB()) {
            notifyOptionChanged("GroundColor");
        }
    }

    float SkyOptions::getHorizonBlend() const {
        return _horizonBlend.load();
    }

    void SkyOptions::setHorizonBlend(float degrees) {
        float clamped = std::max(0.0f, std::min(90.0f, degrees));
        if (_horizonBlend.exchange(clamped) != clamped) {
            notifyOptionChanged("HorizonBlend");
        }
    }

    bool SkyOptions::isSunDiscEnabled() const {
        return _sunDiscEnabled.load();
    }

    void SkyOptions::setSunDiscEnabled(bool enabled) {
        if (_sunDiscEnabled.exchange(enabled) != enabled) {
            notifyOptionChanged("SunDiscEnabled");
        }
    }

    std::string SkyOptions::getShaderSource() const {
        std::lock_guard<std::mutex> lock(_shaderSourceMutex);
        return _shaderSource;
    }

    void SkyOptions::setShaderSource(const std::string& shaderSource) {
        {
            std::lock_guard<std::mutex> lock(_shaderSourceMutex);
            if (_shaderSource == shaderSource) {
                return;
            }
            _shaderSource = shaderSource;
        }
        notifyOptionChanged("ShaderSource");
    }

    void SkyOptions::registerOnChangeListener(const std::shared_ptr<OnChangeListener>& listener) {
        std::lock_guard<std::mutex> lock(_onChangeListenersMutex);
        _onChangeListeners.push_back(listener);
    }

    void SkyOptions::unregisterOnChangeListener(const std::shared_ptr<OnChangeListener>& listener) {
        std::lock_guard<std::mutex> lock(_onChangeListenersMutex);
        _onChangeListeners.erase(std::remove(_onChangeListeners.begin(), _onChangeListeners.end(), listener), _onChangeListeners.end());
    }

    void SkyOptions::notifyOptionChanged(const std::string& optionName) {
        std::vector<std::shared_ptr<OnChangeListener> > onChangeListeners;
        {
            std::lock_guard<std::mutex> lock(_onChangeListenersMutex);
            onChangeListeners = _onChangeListeners;
        }
        for (const std::shared_ptr<OnChangeListener>& listener : onChangeListeners) {
            listener->onSkyOptionChanged(optionName);
        }
    }
}
