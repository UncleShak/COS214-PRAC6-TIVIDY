/**
 * @file Log.h
 * @brief Debug log written to a file.
 *
 * While ncurses owns the terminal nothing may be written to std::cout or std::cerr,
 * so all diagnostic output goes here. Logging is off until open() is called.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_LOG_H
#define TIVIDY_LOG_H

#include <string>

namespace tividy {
namespace ui {

/** @brief Process-wide append-only debug log. */
class Log {
public:
    /** @brief Starts logging to @p path (appends). @return false if the file cannot be opened. */
    static bool open(const std::string& path);
    /** @brief Writes one timestamped line. Does nothing if the log is not open. */
    static void write(const std::string& message);
    static void close();

private:
    Log();
};

} // namespace ui
} // namespace tividy

#endif
