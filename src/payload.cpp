#include "PCH.h"
#include "payload.h"

namespace arrow {
    //helpers for trimming strings
    static bool ieq(std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            unsigned char ac = static_cast<unsigned char>(a[i]);
            unsigned char bc = static_cast<unsigned char>(b[i]);
            if (std::tolower(ac) != std::tolower(bc)) return false;
        }
        return true;
    }

    bool IsSep(char c) { return c == ' ' || c == '\t' || c == ',' || c == ';'; }
    std::string_view Trim(std::string_view s) {
        while (!s.empty() && std::isspace((unsigned char)s.front())) s.remove_prefix(1);
        while (!s.empty() && std::isspace((unsigned char)s.back())) s.remove_suffix(1);
        return s;
    }
    std::optional<float> ParseFloat(std::string_view s) {
        float out{};
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
        if (ec != std::errc{} || ptr == s.data()) return std::nullopt;
        return out;
    }

    //defaults to sending 1 if incorrect
    int ParseInt(std::string_view s) {
        int out{};
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
        if (ec != std::errc{} || ptr == s.data()) return 1;
        return out;
    }

    arrowPayload process(std::string_view payload) {
        arrowPayload output{};
        payload = Trim(payload);
        /*check for:
            count=(int)arrowCount
            spread=(float)spreadAngle
            stamina=(float)staminaCost
        */
        while (!payload.empty()) {
            while (!payload.empty() && IsSep(payload.front())) payload.remove_prefix(1);

            size_t end = 0;

            while (end < payload.size() && !IsSep(payload[end])) ++end;

            auto token = payload.substr(0, end);
            payload.remove_prefix(end);

            auto eq = token.find('=');
            if (eq == std::string_view::npos) continue;

            auto key = Trim(token.substr(0, eq));
            auto val = Trim(token.substr(eq + 1));

            if (key == "count")
                output.arrowCount = ParseInt(val);
            else if (key == "stamina")
                output.staminaCost = ParseFloat(val);
            else if (key == "spread")
                output.spreadAngle = ParseFloat(val);
        }
        
	}
}