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

    struct payloadLimits {
        float minimumDamage{0.0f};
        float maximumDamage{10.0f};

        std::uint32_t minimumCount{1};
        std::uint32_t maximumCount{15};

        float minimumSpread{0.0f};
        float maximumSpread{360.0f};

        std::uint32_t minimumConsume{0};
        std::uint32_t maximumConsume{100};

        float minimumDuration{0.3f};
        float maximumDuration{5.0f};

        float minimumApex{256.0f};
        float maximumApex{4095.0f};
    };
    arrowPayload process(std::string_view payload, const options& options = {});
}