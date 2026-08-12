#include "useraudit/event_serializer.hpp"

#include <cstdio>
#include <sstream>
#include <string_view>

namespace useraudit {

namespace {

void append_escaped(std::string& out, std::string_view input) {
    out.push_back('"');
    for (const unsigned char ch : input) {
        switch (ch) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (ch < 0x20) {
                    char buf[7]{};
                    snprintf(buf, sizeof(buf), "\\u%04x", ch);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(ch));
                }
                break;
        }
    }
    out.push_back('"');
}

void append_field(std::string& out, std::string_view key, std::string_view value, bool& first) {
    if (!first) {
        out.push_back(',');
    }
    first = false;
    append_escaped(out, key);
    out.push_back(':');
    append_escaped(out, value);
}

void append_field_int(std::string& out, std::string_view key, int value, bool& first) {
    if (!first) {
        out.push_back(',');
    }
    first = false;
    append_escaped(out, key);
    out.push_back(':');
    out += std::to_string(value);
}

}  // namespace

std::string json_escape(std::string_view input) {
    std::string out;
    append_escaped(out, input);
    return out;
}

std::string serialize_event_json(const AuditEvent& event) {
    std::string out;
    out.reserve(512);
    out.push_back('{');

    bool first = true;
    append_field(out, "id", event.id, first);
    append_field(out, "ts", event.ts, first);
    append_field_int(out, "lvl", event.lvl, first);
    append_field(out, "cat", event.cat, first);
    append_field(out, "act", event.act, first);
    append_field(out, "sev", event.sev, first);
    append_field(out, "host", event.host, first);

    if (!event.user.empty()) {
        append_field(out, "user", event.user, first);
    }
    if (!event.sid.empty()) {
        append_field(out, "sid", event.sid, first);
    }
    if (event.sess != 0) {
        append_field_int(out, "sess", event.sess, first);
    }
    append_field(out, "src", event.src, first);
    if (!event.corr.empty()) {
        append_field(out, "corr", event.corr, first);
    }

    if (!event.data.empty()) {
        if (!first) {
            out.push_back(',');
        }
        first = false;
        append_escaped(out, "data");
        out.push_back(':');
        out.push_back('{');
        bool data_first = true;
        for (const auto& [key, value] : event.data) {
            append_field(out, key, value, data_first);
        }
        out.push_back('}');
    }

    out.push_back('}');
    return out;
}

}  // namespace useraudit
