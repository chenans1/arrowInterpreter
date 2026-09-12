#include "PCH.h"
#include "payload.h"

namespace arrow {
    //helpers for trimming strings

    arrowPayload process(std::string_view payload, const options& options) { 
        // arrowPayload result{};
        arrowPayload result = options.defaults;
        bool consumeSpecified = false;
        while (!payload.empty()) {
            const auto separator = payload.find('|');

            std::string_view token;

            if (separator == std::string_view::npos) {
                token = payload;
                payload = {};
            } else {
                token = payload.substr(0, separator);
                payload.remove_prefix(separator + 1);
            }

            const auto equals = token.find('=');
            if (equals == std::string_view::npos) {
                continue;
            }

            const auto key = token.substr(0, equals);
            const auto valueString = token.substr(equals + 1);

            if (valueString.empty()) {
                continue;
            }

            const char* begin = valueString.data();
            const char* end = begin + valueString.size();

            if (key == "dmg") {
                float value{};

                const auto [ptr, error] = std::from_chars(begin, end, value);

                if (error == std::errc{} && ptr == end) {
                    result.damageMult = value;
                }
            } else if (key == "spread") {
                float value{};

                const auto [ptr, error] = std::from_chars(begin, end, value);

                if (error == std::errc{} && ptr == end && value >= 0.0f && value <= options.maximumSpread) {
                    result.spread = value;
                }
            } else if (key == "count") {
                std::uint32_t value{};

                const auto [ptr, error] = std::from_chars(begin, end, value);

                if (error == std::errc{} && ptr == end && value >= 1 && value <= options.maximumCount) {
                    result.count = value;
                }
            } else if (key == "consume") {
                std::uint32_t value{};

                const auto [ptr, error] = std::from_chars(begin, end, value);

                if (error == std::errc{} && ptr == end) {
                    result.consume = value;
                    consumeSpecified = true;
                }
            } else if (key == "duration") {
                float value{};

                const auto [ptr, error] = std::from_chars(begin, end, value);

                if (error == std::errc{} && ptr == end && value >= 0.3f && value <= 5.0f) {
                    result.flightDuration = value;
                }
            } else if (key == "apex") {
                float value{};

                const auto [ptr, error] = std::from_chars(begin, end, value);

                if (error == std::errc{} && ptr == end && value >= 256.0f && value <= 4095.0f) {
                    result.apex = value;
                }
            }
        }

        if (!consumeSpecified && options.consumeDefaultsToCount) {
            result.consume = result.count;
        }
        return result;
    }
}