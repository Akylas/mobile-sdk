#include "MapBoxElevationDataDecoder.h"

namespace carto
{

    MapBoxElevationDataDecoder::MapBoxElevationDataDecoder() :
        ElevationDecoder()
    {
    }

    MapBoxElevationDataDecoder::~MapBoxElevationDataDecoder()
    {
    }

    std::array<double, 4> MapBoxElevationDataDecoder::getColorComponentCoefficients() const
    {
        return COMPONENTS;
    }

    std::array<float, 4> MapBoxElevationDataDecoder::getVectorTileScales() const
    {
        return SCALES;
    }

    float MapBoxElevationDataDecoder::getMinimumHeightScale() const
    {
        return 0.1f; // MapBox RGB format uses 0.1 meter as the base unit (smallest coefficient)
    }

    const std::array<double, 4> MapBoxElevationDataDecoder::COMPONENTS = std::array<double, 4>{256 * 256 * 0.1f, 256 * 0.1f, 0.1f, -10000.0f};
    const std::array<float, 4> MapBoxElevationDataDecoder::SCALES = std::array<float, 4>{256 * 256, 256.0f, 1.0f, 0.0f};
} // namespace carto
