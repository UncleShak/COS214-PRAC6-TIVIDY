#include "Log.h"

#include <ctime>
#include <fstream>

namespace tividy {
namespace ui {

namespace {
std::ofstream& stream() {
    static std::ofstream file;
    return file;
}
} // namespace

bool Log::open(const std::string& path) {
    close();
    stream().open(path.c_str(), std::ios::app);
    return stream().is_open();
}

void Log::write(const std::string& message) {
    if (!stream().is_open()) {
        return;
    }
    char stamp[32];
    std::time_t now = std::time(0);
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    stream() << stamp << "  " << message << '\n';
    stream().flush();
}

void Log::close() {
    if (stream().is_open()) {
        stream().close();
    }
}

} // namespace ui
} // namespace tividy
