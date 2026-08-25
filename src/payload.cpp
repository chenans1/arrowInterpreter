#include "PCH.h"
#include "payload.h"

namespace arrow {
    //helpers for trimming strings

    arrowPayload process(std::string_view payload) { 
        arrowPayload result{};

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

                if (error == std::errc{} && ptr == end && value >= 0.0f && value <= 360.0f) {
                    result.spread = value;
                }
            } else if (key == "count") {
                std::uint32_t value{};

                const auto [ptr, error] = std::from_chars(begin, end, value);

                if (error == std::errc{} && ptr == end && value >= 1 && value <= 15) {
                    result.count = value;
                }
            } else if (key == "consume") {
                std::uint32_t value{};

                const auto [ptr, error] = std::from_chars(begin, end, value);

                if (error == std::errc{} && ptr == end) {
                    result.consume = value;
                    consumeSpecified = true;
                }
            }
        }

        if (!consumeSpecified) {
            result.consume = result.count;
        }
        return result;
    }
}