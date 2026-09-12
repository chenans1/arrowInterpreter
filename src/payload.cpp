#include "PCH.h"
#include "payload.h"
#include "payloadAlias.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace arrow {
    //helpers for trimming strings

    arrowPayload process(std::string_view payload, const options& options) { 
        // arrowPayload result{};
        payloadLimits limits{};
        limits.maximumSpread = options.maximumSpread;
        limits.maximumCount = options.maximumCount;
        arrowPayload result = options.defaults;

        bool consumeSpecified = false;
        if (payload.starts_with('$')) {
            const auto separator = payload.find('|');
            const auto aliasName = payload.substr(0, separator);

            if (const auto aliasOverride = payloadAlias::find(aliasName)) {
                result.damageMult = aliasOverride->damageMult.value_or(result.damageMult);
                result.count = aliasOverride->count.value_or(result.count);
                result.spread = aliasOverride->spread.value_or(result.spread);
                result.flightDuration = aliasOverride->duration.value_or(result.flightDuration);
                result.apex = aliasOverride->apex.value_or(result.apex);

                if (aliasOverride->consume) {
                    result.consume = *aliasOverride->consume;
                    consumeSpecified = true;
                }
            }
        }

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

                if (error == std::errc{} && ptr == end && value >= limits.minimumDamage && value <= limits.maximumDamage) {
                    result.damageMult = value;
                }
            } else if (key == "spread") {
                float value{};

                const auto [ptr, error] = std::from_chars(begin, end, value);

                if (error == std::errc{} && ptr == end && value >= limits.minimumSpread && value <= limits.maximumSpread) {
                    result.spread = value;
                }
            } else if (key == "count") {
                std::uint32_t value{};

                const auto [ptr, error] = std::from_chars(begin, end, value);

                if (error == std::errc{} && ptr == end && value >= limits.minimumCount && value <= limits.maximumCount) {
                    result.count = value;
                }
            } else if (key == "consume") {
                std::uint32_t value{};

                const auto [ptr, error] = std::from_chars(begin, end, value);

                if (error == std::errc{} && ptr == end && value >= limits.minimumConsume && value <= limits.maximumConsume) {
                    result.consume = value;
                    consumeSpecified = true;
                }
            } else if (key == "duration") {
                float value{};

                const auto [ptr, error] = std::from_chars(begin, end, value);

                if (error == std::errc{} && ptr == end && value >= limits.minimumDuration && value <= limits.maximumDuration) {
                    result.flightDuration = value;
                }
            } else if (key == "apex") {
                float value{};

                const auto [ptr, error] = std::from_chars(begin, end, value);

                if (error == std::errc{} && ptr == end && value >= limits.minimumApex && value <= limits.maximumApex) {
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
