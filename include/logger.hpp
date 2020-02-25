#ifndef EQNIX_LOGGER_HPP
#define EQNIX_LOGGER_HPP

#include <string>
#include <glib.h>

namespace logging {

class EqnixLogger {
public:
    EqnixLogger(std::string name);
    ~EqnixLogger();

    static EqnixLogger create(std::string name);

    void debug(const std::string& msg);
    void info(const std::string& msg);
    void warn(const std::string& msg);
    void error(const std::string& msg);
    void critical(const std::string& msg);
private:
    std::string logname;
};

static EqnixLogger defaultLogger = EqnixLogger::create("eqnix");

} // namespace log

#endif // EQNIX_LOGGER_HPP