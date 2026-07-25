#pragma once

namespace arrow {
    struct arrowPayload {
        int arrowCount;
        std::optional<float> spreadAngle;
        std::optional<float> damageMult;
    };

    arrowPayload process(std::string_view payload);
}