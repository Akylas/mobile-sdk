#include "TerrariumElevationDataDecoder.h"

namespace carto
{

    TerrariumElevationDataDecoder::TerrariumElevationDataDecoder() :
        ElevationDecoder()
    {
    }

    TerrariumElevationDataDecoder::~TerrariumElevationDataDecoder()
    {
    }

    std::array<double, 4> TerrariumElevationDataDecoder::getColorComponentCoefficients() const
    {
        return COMPONENTS;
    }

    std::array<float, 4> TerrariumElevationDataDecoder::getVectorTileScales() const
    {
        return SCALES;
    }

    float TerrariumElevationDataDecoder::getMinimumHeightScale() const
    {
        return 1.0f / 256.0f; // Terrarium format uses 1/256 meter as the base unit (smallest coefficient)
    }

    const std::array<double, 4> TerrariumElevationDataDecoder::COMPONENTS = std::array<double, 4>{256.0f, 1.0f, 1.0f / 256, -32768.0f};
    const std::array<float, 4> TerrariumElevationDataDecoder::SCALES = std::array<float, 4>{256 * 256, 256.0f, 1.0f, 0.0f};
} // namespace carto
