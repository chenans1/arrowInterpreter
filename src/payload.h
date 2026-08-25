#pragma once

namespace arrow {
    struct arrowPayload {
        float damageMult{1.0f};
        std::uint32_t count{1};
        float spread{0.0f};
        std::uint32_t consume{1};
    };

    arrowPayload process(std::string_view payload);
}