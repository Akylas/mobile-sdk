#include "TerrainOptions.h"
#include "components/Exceptions.h"
#include "datasources/TileDataSource.h"
#include "terrain/ElevationManager.h"

#include <algorithm>

namespace carto {

    TerrainOptions::TerrainOptions(const std::shared_ptr<TileDataSource>& dataSource) :
        TerrainOptions(dataSource, std::shared_ptr<ElevationDecoder>())
    {
    }

    TerrainOptions::TerrainOptions(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<ElevationDecoder>& elevationDecoder) :
        _dataSource(dataSource),
        _elevationManager(dataSource ? std::make_shared<ElevationManager>(dataSource, elevationDecoder) : std::shared_ptr<ElevationManager>()),
        _enabled(true),
        _meshResolution(32),
        _regularGridEnabled(false),
        _minZoom(5),
        _maxTileZoomOffset(100),
        _backgroundColorARGB(0),
        _backgroundBitmapEnabled(false),
        _depthBias(0.0002f),
        _cameraClearance(200.0f),
        _cameraClampDuration(0.0f),
        _billboardOcclusionEnabled(true),
        _onChangeListeners(),
        _onChangeListenersMutex()
    {
        if (!dataSource) {
            throw NullArgumentException("Null dataSource");
        }
    }

    TerrainOptions::~TerrainOptions() {
    }

    std::shared_ptr<TileDataSource> TerrainOptions::getDataSource() const {
        return _dataSource;
    }

    std::shared_ptr<ElevationDecoder> TerrainOptions::getElevationDecoder() const {
        return _elevationManager->getElevationDecoder();
    }

    bool TerrainOptions::isEnabled() const {
        return _enabled.load();
    }

    void TerrainOptions::setEnabled(bool enabled) {
        if (_enabled.exchange(enabled) != enabled) {
            notifyOptionChanged("Enabled");
        }
    }

    float TerrainOptions::getExaggeration() const {
        return _elevationManager->getExaggeration();
    }

    void TerrainOptions::setExaggeration(float exaggeration) {
        if (_elevationManager->getExaggeration() != exaggeration) {
            _elevationManager->setExaggeration(exaggeration);
            notifyOptionChanged("Exaggeration");
        }
    }

    int TerrainOptions::getMeshResolution() const {
        return _meshResolution.load();
    }

    void TerrainOptions::setMeshResolution(int meshResolution) {
        int resolution = std::min(256, std::max(2, meshResolution));
        if (_meshResolution.exchange(resolution) != resolution) {
            notifyOptionChanged("MeshResolution");
        }
    }

    bool TerrainOptions::isRegularGridEnabled() const {
        return _regularGridEnabled.load();
    }

    void TerrainOptions::setRegularGridEnabled(bool regularGridEnabled) {
        if (_regularGridEnabled.exchange(regularGridEnabled) != regularGridEnabled) {
            notifyOptionChanged("RegularGridEnabled");
        }
    }

    int TerrainOptions::getMinZoom() const {
        return _minZoom.load();
    }

    void TerrainOptions::setMinZoom(int minZoom) {
        int zoom = std::min(24, std::max(0, minZoom));
        if (_minZoom.exchange(zoom) != zoom) {
            notifyOptionChanged("MinZoom");
        }
    }

    Color TerrainOptions::getBackgroundColor() const {
        return Color(_backgroundColorARGB.load());
    }

    void TerrainOptions::setBackgroundColor(const Color& color) {
        int value = color.getARGB();
        if (_backgroundColorARGB.exchange(value) != value) {
            notifyOptionChanged("BackgroundColor");
        }
    }

    bool TerrainOptions::isBackgroundBitmapEnabled() const {
        return _backgroundBitmapEnabled.load();
    }

    void TerrainOptions::setBackgroundBitmapEnabled(bool enabled) {
        if (_backgroundBitmapEnabled.exchange(enabled) != enabled) {
            notifyOptionChanged("BackgroundBitmapEnabled");
        }
    }

    int TerrainOptions::getMaxTileZoomOffset() const {
        return _maxTileZoomOffset.load();
    }

    void TerrainOptions::setMaxTileZoomOffset(int offset) {
        int value = std::min(100, std::max(-24, offset));
        if (_maxTileZoomOffset.exchange(value) != value) {
            notifyOptionChanged("MaxTileZoomOffset");
        }
    }

    float TerrainOptions::getCameraClearance() const {
        return _cameraClearance.load();
    }

    void TerrainOptions::setCameraClearance(float clearance) {
        float value = std::max(0.0f, clearance);
        if (_cameraClearance.exchange(value) != value) {
            notifyOptionChanged("CameraClearance");
        }
    }

    float TerrainOptions::getCameraClampDuration() const {
        return _cameraClampDuration.load();
    }

    void TerrainOptions::setCameraClampDuration(float duration) {
        float value = std::max(0.0f, duration);
        if (_cameraClampDuration.exchange(value) != value) {
            notifyOptionChanged("CameraClampDuration");
        }
    }

    float TerrainOptions::getDepthBias() const {
        return _depthBias.load();
    }

    void TerrainOptions::setDepthBias(float depthBias) {
        float bias = std::min(0.01f, std::max(0.0f, depthBias));
        if (_depthBias.exchange(bias) != bias) {
            notifyOptionChanged("DepthBias");
        }
    }

    bool TerrainOptions::isBillboardOcclusionEnabled() const {
        return _billboardOcclusionEnabled.load();
    }

    void TerrainOptions::setBillboardOcclusionEnabled(bool enabled) {
        if (_billboardOcclusionEnabled.exchange(enabled) != enabled) {
            notifyOptionChanged("BillboardOcclusionEnabled");
        }
    }

    std::size_t TerrainOptions::getElevationCacheCapacity() const {
        return _elevationManager->getCacheCapacity();
    }

    void TerrainOptions::setElevationCacheCapacity(std::size_t capacityInBytes) {
        _elevationManager->setCacheCapacity(capacityInBytes);
    }

    double TerrainOptions::getElevation(const MapPos& pos) const {
        return _elevationManager->getElevation(pos);
    }

    std::vector<double> TerrainOptions::getElevations(const std::vector<MapPos>& poses) const {
        return _elevationManager->getElevations(poses);
    }

    std::shared_ptr<ElevationManager> TerrainOptions::getElevationManager() const {
        return _elevationManager;
    }

    void TerrainOptions::registerOnChangeListener(const std::shared_ptr<OnChangeListener>& listener) {
        std::lock_guard<std::mutex> lock(_onChangeListenersMutex);
        _onChangeListeners.push_back(listener);
    }

    void TerrainOptions::unregisterOnChangeListener(const std::shared_ptr<OnChangeListener>& listener) {
        std::lock_guard<std::mutex> lock(_onChangeListenersMutex);
        _onChangeListeners.erase(std::remove(_onChangeListeners.begin(), _onChangeListeners.end(), listener), _onChangeListeners.end());
    }

    void TerrainOptions::notifyOptionChanged(const std::string& optionName) {
        std::vector<std::shared_ptr<OnChangeListener> > onChangeListeners;
        {
            std::lock_guard<std::mutex> lock(_onChangeListenersMutex);
            onChangeListeners = _onChangeListeners;
        }
        for (const std::shared_ptr<OnChangeListener>& listener : onChangeListeners) {
            listener->onTerrainOptionChanged(optionName);
        }
    }
}
