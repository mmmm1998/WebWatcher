#include "logging.hpp"

const std::string Log::kLoggingInfoFormat = "[%s:%u][%s] ";
Log::Level Log::m_current_level = Log::Level::Debug;

void Log::setLevel(Level level) noexcept {
    m_current_level = level;
}
