#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace payloadAlias {
    struct PayloadOverride{
        std::optional<float> damageMult;
        std::optional<std::uint32_t> count;
        std::optional<float> spread;
        std::optional<std::uint32_t> consume;
        std::optional<float> duration;
        std::optional<float> apex;
    };

    using AliasMap = std::unordered_map<std::string, PayloadOverride>;

    extern AliasMap aliases;

    void Load();

    std::optional<PayloadOverride> find(std::string_view alias);
}
