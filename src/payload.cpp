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
            if (const auto aliasOverride = payloadAlias::find(payload)) {
                if (aliasOverride->damageMult &&
                    *aliasOverride->damageMult >= limits.minimumDamage &&
                    *aliasOverride->damageMult <= limits.maximumDamage) {
                    result.damageMult = *aliasOverride->damageMult;
                }
                if (aliasOverride->count &&
                    *aliasOverride->count >= limits.minimumCount &&
                    *aliasOverride->count <= limits.maximumCount) {
                    result.count = *aliasOverride->count;
                }
                if (aliasOverride->spread &&
                    *aliasOverride->spread >= limits.minimumSpread &&
                    *aliasOverride->spread <= limits.maximumSpread) {
                    result.spread = *aliasOverride->spread;
                }
                if (aliasOverride->consume &&
                    *aliasOverride->consume >= limits.minimumConsume &&
                    *aliasOverride->consume <= limits.maximumConsume) {
                    result.consume = *aliasOverride->consume;
                    consumeSpecified = true;
                }
                if (aliasOverride->duration &&
                    *aliasOverride->duration >= limits.minimumDuration &&
                    *aliasOverride->duration <= limits.maximumDuration) {
                    result.flightDuration = *aliasOverride->duration;
                }
                if (aliasOverride->apex &&
                    *aliasOverride->apex >= limits.minimumApex &&
                    *aliasOverride->apex <= limits.maximumApex) {
                    result.apex = *aliasOverride->apex;
                }
            }
        } else {
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
        }

        if (!consumeSpecified && options.consumeDefaultsToCount) {
            result.consume = result.count;
        }
        return result;
    }
}
