#include "PCH.h"
#include "payloadAlias.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace payloadAlias {
    AliasMap aliases;

    namespace {
        std::optional<float> ReadFloat(const nlohmann::json& value) {
            float result{};
            if (value.is_number()) {
                result = value.get<float>();
            } else if (value.is_string()) {
                const auto& text = value.get_ref<const std::string&>();
                const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
                if (error != std::errc{} || end != text.data() + text.size()) {
                    return std::nullopt;
                }
            } else {
                return std::nullopt;
            }

            return std::isfinite(result) ? std::optional{ result } : std::nullopt;
        }

        std::optional<std::uint32_t> ReadUnsigned(const nlohmann::json& value) {
            if (value.is_number_unsigned()) {
                const auto result = value.get<std::uint64_t>();
                if (result <= std::numeric_limits<std::uint32_t>::max()) {
                    return static_cast<std::uint32_t>(result);
                }
                return std::nullopt;
            }

            if (value.is_number_integer()) {
                const auto result = value.get<std::int64_t>();
                if (result >= 0 && static_cast<std::uint64_t>(result) <= std::numeric_limits<std::uint32_t>::max()) {
                    return static_cast<std::uint32_t>(result);
                }
                return std::nullopt;
            }

            if (value.is_string()) {
                const auto& text = value.get_ref<const std::string&>();
                std::uint32_t result{};
                const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
                if (error == std::errc{} && end == text.data() + text.size()) {
                    return result;
                }
            }

            return std::nullopt;
        }

        void ReadFloatMember(const nlohmann::json& object, const char* key, std::optional<float>& destination) {
            if (const auto member = object.find(key); member != object.end()) {
                destination = ReadFloat(*member);
            }
        }

        void ReadUnsignedMember(const nlohmann::json& object, const char* key, std::optional<std::uint32_t>& destination) {
            if (const auto member = object.find(key); member != object.end()) {
                destination = ReadUnsigned(*member);
            }
        }

        PayloadOverride ReadOverride(const nlohmann::json& object) {
            PayloadOverride result;
            ReadFloatMember(object, "damage", result.damageMult);
            ReadFloatMember(object, "spread", result.spread);
            ReadFloatMember(object, "apex", result.apex);
            ReadFloatMember(object, "duration", result.duration);
            ReadUnsignedMember(object, "count", result.count);
            ReadUnsignedMember(object, "consume", result.consume);
            return result;
        }
        

    }

    void Load() {
        const auto configDirectory = std::filesystem::path(REL::Module::get().filePath()).parent_path() / "Data/SKSE/Plugins/AIE";

        log::info("[PayloadAlias] Scanning {}", configDirectory.string());

        std::error_code error;
        if (!std::filesystem::exists(configDirectory, error)) {
            log::info("[PayloadAlias] Directory does not exist; no aliases loaded");
            return;
        }
        if (error || !std::filesystem::is_directory(configDirectory, error)) {
            SKSE::log::warn("[PayloadAlias] Config path is not a readable directory: {}", configDirectory.string());
            return;
        }

        std::vector<std::filesystem::path> configFiles;
        for (std::filesystem::directory_iterator iterator(configDirectory, error), end;
             !error && iterator != end;
             iterator.increment(error)) {
            const auto& entry = *iterator;
            if (!entry.is_regular_file(error)) {
                error.clear();
                continue;
            }

            const auto filename = entry.path().filename().string();
            if (filename.ends_with(".json")) {
                configFiles.push_back(entry.path());
            }
        }

        if (error) {
            log::warn("[PayloadAlias] Failed while enumerating {}: {}", configDirectory.string(), error.message());
            return;
        }

        std::ranges::sort(configFiles);

        std::size_t loadedFiles = 0;
        std::size_t loadedAliases = 0;
        for (const auto& path : configFiles) {
            try {
                std::ifstream stream(path);
                if (!stream) {
                    log::warn("[PayloadAlias] Could not open {}", path.filename().string());
                    continue;
                }

                const auto document = nlohmann::json::parse(stream);
                const auto aliasObject = document.find("aliases");
                if (aliasObject == document.end() || !aliasObject->is_object()) {
                    log::warn("[PayloadAlias] {} has no aliases object", path.filename().string());
                    continue;
                }

                std::size_t fileAliases = 0;
                for (const auto& [aliasName, aliasValue] : aliasObject->items()) {
                    if (aliasName.empty() || aliasName.front() != '$' || !aliasValue.is_object()) {
                        log::warn("[PayloadAlias] Ignoring invalid alias '{}' in {}", aliasName, path.filename().string());
                        continue;
                    }

                    aliases.insert_or_assign(aliasName, ReadOverride(aliasValue));
                    ++fileAliases;
                }

                log::info("[PayloadAlias] Loaded {} ({} alias(es))", path.filename().string(), fileAliases);
                loadedAliases += fileAliases;
                ++loadedFiles;
            } catch (const nlohmann::json::exception& exception) {
                log::warn( "[PayloadAlias] Invalid JSON in {}: {}", path.filename().string(), exception.what());
            }
        }

        log::info("[PayloadAlias] Finished loading: {} matching file(s), {} parsed successfully, {} alias definition(s)",
            configFiles.size(), loadedFiles, loadedAliases);
    }

    std::optional<PayloadOverride> find(std::string_view alias) {
        const auto found = aliases.find(std::string(alias));
        if (found == aliases.end()) {
            log::info("[PayloadAlias] {} alias not found", alias);
            return std::nullopt;
        }

        return found->second;
    }
}
