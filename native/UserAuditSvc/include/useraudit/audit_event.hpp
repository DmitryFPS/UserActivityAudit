#pragma once

#include <map>
#include <string>

namespace useraudit {

struct AuditEvent {
    std::string id;
    std::string ts;
    int lvl = 1;
    std::string cat;
    std::string act;
    std::string sev = "info";
    std::string host;
    std::string user;
    std::string sid;
    int sess = 0;
    std::string src = "eventlog";
    std::string corr;
    std::map<std::string, std::string> data;
};

}  // namespace useraudit
