#include "PCH.h"
#include "payloadAlias.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using namespace SKSE;
using namespace SKSE::log;
using namespace std::literals;

namespace payloadAlias {
    void Load()
    {
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
        for (const auto& path : configFiles) {
            try {
                std::ifstream stream(path);
                if (!stream) {
                    log::warn("[PayloadAlias] Could not open {}", path.filename().string());
                    continue;
                }

                const auto document = nlohmann::json::parse(stream);
                log::info("[PayloadAlias] Loaded {} ({} top-level entries)", path.filename().string(), document.size());
                ++loadedFiles;
            } catch (const nlohmann::json::exception& exception) {
                log::warn( "[PayloadAlias] Invalid JSON in {}: {}", path.filename().string(), exception.what());
            }
        }

        log::info("[PayloadAlias] Finished loading: {} matching file(s) found, {} parsed successfully", configFiles.size(), loadedFiles);
    }
}
