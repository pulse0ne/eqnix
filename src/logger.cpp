#include "logger.hpp"

namespace logging {

EqnixLogger::EqnixLogger(std::string name) : logname(name) {
    //
}

EqnixLogger::~EqnixLogger() {}

EqnixLogger EqnixLogger::create(std::string name) {
    return EqnixLogger(name);
}

void EqnixLogger::debug(const std::string& s) {
    g_debug("[%s] %s", logname.c_str(), s.c_str());
}

void EqnixLogger::info(const std::string& s) {
    g_info("[%s] %s", logname.c_str(), s.c_str());
}

void EqnixLogger::warn(const std::string& s) {
    g_warning("[%s] %s", logname.c_str(), s.c_str());
}

void EqnixLogger::error(const std::string& s) {
    g_error("[%s] %s", logname.c_str(), s.c_str());
}

void EqnixLogger::critical(const std::string& s) {
    g_critical("[%s] %s", logname.c_str(), s.c_str());
}

} // namespace log
