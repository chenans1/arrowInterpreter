#pragma once

#include <cstdint>
#include <optional>

namespace payloadAlias {
    struct PayloadOverride{
        std::optional<float> damageMult;
        std::optional<std::uint32_t> count;
        std::optional<float> spread;
        std::optional<std::uint32_t> consume;
        std::optional<float> flightDuration;
        std::optional<float> apex;
    };

    void Load();
}
