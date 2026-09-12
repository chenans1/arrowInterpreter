#pragma once

namespace arrow {
    struct arrowPayload {
        float damageMult{1.0f};
        std::uint32_t count{1};
        float spread{0.0f};
        std::uint32_t consume{1};
        float flightDuration{1.0f};
        float apex{512.0f};
    };

    struct options {
        arrowPayload defaults{};
        float maximumSpread{360.0f};
        std::uint32_t maximumCount{15};
        bool consumeDefaultsToCount{true};
    };

    arrowPayload process(std::string_view payload, const options& options = {});
}