#include "HillshadeRasterTileLayer.h"
#include "renderers/MapRenderer.h"
#include "renderers/TileRenderer.h"
#include "utils/Log.h"
#include "utils/TileUtils.h"
#include "core/BinaryData.h"
#include "core/Variant.h"
#include "datasources/components/TileData.h"
#include "rastertiles/TerrariumElevationDataDecoder.h"
#include "rastertiles/MapBoxElevationDataDecoder.h"
#include "projections/EPSG3857.h"
#include "projections/Projection.h"
#include "terrain/ElevationManager.h"

#include <array>
#include <algorithm>

#include "graphics/Bitmap.h"

#include <vt/TileId.h>
#include <vt/Tile.h>
#include <vt/TileTransformer.h>
#include <vt/TileBitmap.h>
#include <vt/TileLayer.h>
#include <vt/TileLayerBuilder.h>
#include <vt/NormalMapBuilder.h>
#include <vt/TileLayerBuilder.h>

namespace carto
{

    HillshadeRasterTileLayer::HillshadeRasterTileLayer(const std::shared_ptr<TileDataSource> &dataSource, const std::shared_ptr<ElevationDecoder> &elevationDecoder) : CustomRasterTileLayer(dataSource),
        _elevationDecoder(elevationDecoder),
        _contrast(0.5f),
        _heightScale(0.09f),
        _exaggeration(1.0f),
        _exagerateHeightScaleEnabled(true),
        _normalMapLightingShader(),
        _accentColor(Color(0, 0, 0, 255)),
        _shadowColor(Color(0, 0, 0, 255)),
        _highlightColor(Color(255, 255, 255, 255)),
        _illuminationDirection(MapVec(-0.42261826, 0.90630779, -0.70710678)),  // azimuth=335°, altitude=45° (MapLibre defaults)
        _illuminationMapRotationEnabled(true),
        _hillshadeMethod(HillshadeMethod::HillshadeMethod::STANDARD),
        _contourEnabled(false),
        _elevationEncodingEnabled(false),
        _contourInterval(100.0f),
        _contourColor(Color(0xC5, 0x60, 0x08, 0xff)),
        _contourWidth(0.75f)
    {
        setTileBlendingSpeed(0.0f);
    }
    HillshadeRasterTileLayer::HillshadeRasterTileLayer(const std::shared_ptr<TileDataSource> &dataSource) : CustomRasterTileLayer(dataSource),
        _elevationDecoder(nullptr),
        _contrast(0.5f),
        _heightScale(1.0f),
        _exaggeration(1.0f),
        _exagerateHeightScaleEnabled(true),
        _normalMapLightingShader(),
        _accentColor(Color(0, 0, 0, 255)),
        _shadowColor(Color(0, 0, 0, 255)),
        _highlightColor(Color(255, 255, 255, 255)),
        _illuminationDirection(MapVec(-0.42261826, 0.90630779, -0.70710678)),  // azimuth=335°, altitude=45° (MapLibre defaults)
        _illuminationMapRotationEnabled(true),
        _hillshadeMethod(HillshadeMethod::HillshadeMethod::STANDARD),
        _contourEnabled(false),
        _elevationEncodingEnabled(false),
        _contourInterval(100.0f),
        _contourColor(Color(0xC5, 0x60, 0x08, 0xff)),
        _contourWidth(0.75f)
    {
        setTileBlendingSpeed(0.0f);
    }

    HillshadeRasterTileLayer::~HillshadeRasterTileLayer()
    {
    }

    float HillshadeRasterTileLayer::getContrast() const
    {
        return _contrast.load();
    }

    void HillshadeRasterTileLayer::setContrast(float contrast) {
        _contrast.store(std::min(1.0f, std::max(0.0f, contrast)));
        updateTiles(false);
    }

    float HillshadeRasterTileLayer::getHeightScale() const {
        return _heightScale.load();
    }

    void HillshadeRasterTileLayer::setHeightScale(float heightScale) {
        _heightScale.store(heightScale);
        updateTiles(false);
    }

    float HillshadeRasterTileLayer::getExaggeration() const {
        return _exaggeration.load();
    }

    void HillshadeRasterTileLayer::setExaggeration(float exaggeration) {
        _exaggeration.store(exaggeration);
        redraw(); // per-frame shader uniform only; no tile re-decode
    }

    Color HillshadeRasterTileLayer::getShadowColor() const {
        return _shadowColor.load();
    }

    void HillshadeRasterTileLayer::setShadowColor(const Color& color) {
        _shadowColor.store(color);
        redraw();
    }
    
    Color HillshadeRasterTileLayer::getAccentColor() const {
        return _accentColor.load();
    }
    
    void HillshadeRasterTileLayer::setAccentColor(const Color &color) {
        _accentColor.store(color);
        redraw();
    }

    Color HillshadeRasterTileLayer::getHighlightColor() const {
        return _highlightColor.load();
    }

    void HillshadeRasterTileLayer::setHighlightColor(const Color& color) {
        _highlightColor.store(color);
        redraw();
    }

    std::string HillshadeRasterTileLayer::getNormalMapLightingShader() const
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _normalMapLightingShader;
    }
    void HillshadeRasterTileLayer::setNormalMapLightingShader(const std::string &shader)
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _normalMapLightingShader = shader;
        redraw();
    }
    MapVec HillshadeRasterTileLayer::getIlluminationDirection() const
    {
        return _illuminationDirection.load();
    }
    void HillshadeRasterTileLayer::setIlluminationDirection(MapVec direction)
    {
        MapVec directionNormalized = direction;
        directionNormalized.normalize();
        _illuminationDirection.store(directionNormalized);
        redraw();
    }
    bool HillshadeRasterTileLayer::getIlluminationMapRotationEnabled() const
    {
        return _illuminationMapRotationEnabled.load();
    }
    void HillshadeRasterTileLayer::setIlluminationMapRotationEnabled(bool enabled)
    {
        _illuminationMapRotationEnabled.store(enabled);
        redraw();
    }
    bool HillshadeRasterTileLayer::getExagerateHeightScaleEnabled() const
    {
        return _exagerateHeightScaleEnabled.load();
    }
    void HillshadeRasterTileLayer::setExagerateHeightScaleEnabled(bool enabled)
    {
        _exagerateHeightScaleEnabled.store(enabled);
        updateTiles(false);
    }

    HillshadeMethod::HillshadeMethod HillshadeRasterTileLayer::getHillshadeMethod() const {
        return _hillshadeMethod.load();
    }

    void HillshadeRasterTileLayer::setHillshadeMethod(HillshadeMethod::HillshadeMethod method) {
        _hillshadeMethod.store(method);
        redraw();
    }

    bool HillshadeRasterTileLayer::isElevationEncodingEnabled() const {
        return _elevationEncodingEnabled.load();
    }

    void HillshadeRasterTileLayer::setElevationEncodingEnabled(bool enabled) {
        _elevationEncodingEnabled.store(enabled);
        updateTiles(false); // format change (elevation packed into the normal map)
    }

    bool HillshadeRasterTileLayer::isContourEnabled() const {
        return _contourEnabled.load();
    }

    void HillshadeRasterTileLayer::setContourEnabled(bool enabled) {
        _contourEnabled.store(enabled);
        // Toggling contours changes the normal map encoding (elevation packed into B/A), so the
        // tiles must be rebuilt (as with setContrast, which is also baked into the normal map).
        updateTiles(false);
    }

    float HillshadeRasterTileLayer::getContourInterval() const {
        return _contourInterval.load();
    }

    void HillshadeRasterTileLayer::setContourInterval(float interval) {
        _contourInterval.store(interval);
        redraw();
    }

    Color HillshadeRasterTileLayer::getContourColor() const {
        return _contourColor.load();
    }

    void HillshadeRasterTileLayer::setContourColor(const Color& color) {
        _contourColor.store(color);
        redraw();
    }

    float HillshadeRasterTileLayer::getContourWidth() const {
        return _contourWidth.load();
    }

    void HillshadeRasterTileLayer::setContourWidth(float width) {
        _contourWidth.store(width);
        redraw();
    }

    bool HillshadeRasterTileLayer::onDrawFrame(float deltaSeconds, BillboardSorter &billboardSorter, const ViewState &viewState)
    {
        updateTileLoadListener();

        if (auto mapRenderer = getMapRenderer())
        {
            float opacity = getOpacity();

            if (opacity < 1.0f)
            {
                mapRenderer->clearAndBindScreenFBO(Color(0, 0, 0, 0), false, false);
            }

            _tileRenderer->setNormalMapLightingShader(getNormalMapLightingShader());
            _tileRenderer->setRasterFilterMode(getRasterFilterMode());
            _tileRenderer->setLayerBlendingSpeed(getTileBlendingSpeed());
            _tileRenderer->setNormalMapShadowColor(getShadowColor());
            _tileRenderer->setNormalMapAccentColor(getAccentColor());
            _tileRenderer->setNormalMapHighlightColor(getHighlightColor());
            _tileRenderer->setNormalMapElevationEncoded(isElevationEncoded());
            _tileRenderer->setNormalMapContourInterval(isContourEnabled() ? getContourInterval() : 0.0f);
            _tileRenderer->setNormalMapContourColor(getContourColor());
            _tileRenderer->setNormalMapContourWidth(getContourWidth());
            _tileRenderer->setNormalIlluminationDirection(getIlluminationDirection());
            _tileRenderer->setNormalIlluminationMapRotationEnabled(getIlluminationMapRotationEnabled());


            int hillshadeMethod = 0;
            switch (getHillshadeMethod()) {
                case HillshadeMethod::HillshadeMethod::STANDARD:
                    hillshadeMethod = 0;
                    break;
                case HillshadeMethod::HillshadeMethod::COMBINED:
                    hillshadeMethod = 1;
                    break;
                case HillshadeMethod::HillshadeMethod::IGOR:
                    hillshadeMethod = 2;
                    break;
                case HillshadeMethod::HillshadeMethod::MULTIDIRECTIONAL:
                    hillshadeMethod = 3;
                    break;
                case HillshadeMethod::HillshadeMethod::BASIC:
                    hillshadeMethod = 4;
                    break;
            }
            _tileRenderer->setHillshadeMethod(hillshadeMethod);
            _tileRenderer->setHillshadeExaggeration(getContrast() * getExaggeration());
            bool refresh = _tileRenderer->onDrawFrame(deltaSeconds, viewState);

            if (opacity < 1.0f)
            {
                mapRenderer->blendAndUnbindScreenFBO(opacity);
            }

            return refresh;
        }
        return false;
    }
    
    std::shared_ptr<vt::Tile> HillshadeRasterTileLayer::createVectorTile(const MapTile& subTile, const MapTile& tile, const std::shared_ptr<TileData>& tileData, const std::shared_ptr<Bitmap>& bitmap, const std::shared_ptr<vt::TileTransformer>& tileTransformer) const {
        std::uint8_t alpha = 0;
        std::array<float, 4> scales;
        std::array<float, 4> elevationCoeffs = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        {
            // Try to get decoder type from tile metadata first, fallback to layer's decoder
            std::shared_ptr<ElevationDecoder> decoder = _elevationDecoder;
            if (tileData) {
                std::shared_ptr<Variant> decoderTypeVariant = tileData->getMetadata("encoding");
                if (decoderTypeVariant && decoderTypeVariant->getType() == VariantType::VARIANT_TYPE_STRING) {
                    std::string decoderType = decoderTypeVariant->getString();
                    // Use static cached decoder instances to avoid repeated allocations
                    if (decoderType == "terrarium") {
                        static std::shared_ptr<ElevationDecoder> terrariumDecoder = std::make_shared<TerrariumElevationDataDecoder>();
                        decoder = terrariumDecoder;
                    } else if (!decoder) {
                        static std::shared_ptr<ElevationDecoder> mapboxDecoder = std::make_shared<MapBoxElevationDataDecoder>();
                        decoder = mapboxDecoder;
                    }
                }
            }
            if (!decoder) {
                static std::shared_ptr<ElevationDecoder> mapboxDecoder = std::make_shared<MapBoxElevationDataDecoder>();
                decoder = mapboxDecoder;
            }
            scales = decoder->getVectorTileScales();
            std::array<double, 4> rawCoeffs = decoder->getColorComponentCoefficients();
            elevationCoeffs = { { static_cast<float>(rawCoeffs[0]), static_cast<float>(rawCoeffs[1]), static_cast<float>(rawCoeffs[2]), static_cast<float>(rawCoeffs[3]) } };
            alpha = static_cast<std::uint8_t>(getContrast() * 255.0f);
            float heightScale = decoder->getMinimumHeightScale();
            float scale = heightScale * static_cast<float>(bitmap->getHeight() * std::pow(2.0, tile.getZoom()) / 40075016.6855785);
            if (_exagerateHeightScaleEnabled) {
                 float exaggeration = tile.getZoom() < 2 ? 0.2f : tile.getZoom() < 5 ? 0.3f : 0.35f;
                 scale = heightScale * 160 * getHeightScale() * static_cast<float>(bitmap->getHeight() * std::pow(2.0, tile.getZoom() * (1 - exaggeration)) / 40075016.6855785);

            }
            std::transform(scales.begin(), scales.end(), scales.begin(), [&scale](float &c) { return c * scale; });
        }
        
        // Build normal map from height map
        vt::TileId vtTileId(tile.getZoom(), tile.getX(), tile.getY());
        vt::TileId vtSubTileId(subTile.getZoom(), subTile.getX(), subTile.getY());
        std::shared_ptr<Bitmap> rgbaBitmap = bitmap->getRGBABitmap();
        auto rgbaBitmapDataPtr = reinterpret_cast<const std::uint32_t*>(rgbaBitmap->getPixelData().data());
        std::vector<std::uint32_t> rgbaBitmapData(rgbaBitmapDataPtr, rgbaBitmapDataPtr + rgbaBitmap->getWidth() * rgbaBitmap->getHeight());
        auto vtBitmap = std::make_shared<vt::Bitmap>(rgbaBitmap->getWidth(), rgbaBitmap->getHeight(), std::move(rgbaBitmapData));
        vt::NormalMapBuilder normalMapBuilder(scales, alpha, isElevationEncoded(), elevationCoeffs);
        std::shared_ptr<const vt::Bitmap> normalMap = normalMapBuilder.buildNormalMapFromHeightMap(vtTileId, vtTileId, vtBitmap);
        auto normalMapDataPtr = reinterpret_cast<const std::uint8_t*>(normalMap->data.data());
        std::vector<std::uint8_t> normalMapData(normalMapDataPtr, normalMapDataPtr + normalMap->data.size() * sizeof(std::uint32_t));
        auto tileBitmap = std::make_shared<vt::TileBitmap>(vt::TileBitmap::Type::NORMALMAP, vt::TileBitmap::Format::RGBA, normalMap->width, normalMap->height, std::move(normalMapData));
        
        // Build vector tile from created normal map
        float tileSize = 256.0f; // 'normalized' tile size in pixels. Not really important
        vt::TileLayerBuilder tileLayerBuilder(std::string(), 0, vtTileId, tileTransformer, tileSize, 1.0f); // Note: the size/scale argument is ignored
        tileLayerBuilder.addBitmap(tileBitmap);
        std::shared_ptr<vt::TileLayer> tileLayer = tileLayerBuilder.buildTileLayer();
        return std::make_shared<vt::Tile>(vtTileId, tileSize, std::vector<std::shared_ptr<vt::TileLayer> > { tileLayer });
    }

    std::shared_ptr<Bitmap> HillshadeRasterTileLayer::getTileDataBitmap(std::shared_ptr<TileData> tileData) const {
        std::shared_ptr<BinaryData> binaryData = tileData->getData();
        if (!binaryData) {
            Log::Error("HillshadeRasterTileLayer::getTileDataBitmap: Null tile binary data");
            return NULL;
        }
        int size = binaryData->size();
        std::shared_ptr<Bitmap> tileBitmap = Bitmap::CreateFromCompressed(binaryData);
        return tileBitmap;
    }

    std::shared_ptr<ElevationManager> HillshadeRasterTileLayer::getElevationManager() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        if (!_elevationManager) {
            _elevationManager = std::make_shared<ElevationManager>(getDataSource(), _elevationDecoder);
        }
        return _elevationManager;
    }

    double HillshadeRasterTileLayer::getElevation(const MapPos &pos) const
    {
        return getElevationManager()->getElevation(pos);
    }

    std::vector<double> HillshadeRasterTileLayer::getElevations(const std::vector<MapPos> poses) const
    {
        return getElevationManager()->getElevations(poses);
    }
} // namespace carto
