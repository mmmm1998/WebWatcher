#ifndef LOGGING_H
#define LOGGING_H

#include <QDebug>
#include <string_view>
#include <experimental/source_location>

class Log
{
    using source_location = std::experimental::source_location;
public:

    enum class Level : int {
		/* A general information about the low-level program workflow (e.g when entering/exiting a
		 * specific function). Entries of this type may be used in places where expectations of a failure
		 * are relatively higher (e.g in operations involving file or network I/O). */
		Debug,
		/* A general information about the high-level program workflow (e.g. when transitioning between high-level
		 * states or when entering/exiting some large or long-running functions). Entries of this type should give a
		 * general understanding of what happens in the program. */
		Info,
		/* An error happened, but the current action can still be completed, though with some negative impact
		 * (e.g. extra performance overhead or partially missing data). */
		Warning,
		/* An error happened which makes completing the current action impossible (e.g. file not found
		 * during texture loading) but does not require immediate program termination. */
		Error,
		/* An error happened which makes further program execution impossible (e.g. segfault). In most
		 * cases this implies (more probably) a bug in the code or (less probably) some external
		 * environment malfunction, such as going out of memory, losing access to critical files,
		 * driver or hardware failure etc. */
		Fatal,

		Off // Not actually a logging level, use it with `setLevel` to disable logging completely
	};

    // Returns the current logging level
    static Level level() noexcept { return m_current_level; }
    // Changes the current logging level
    static void setLevel(Level value) noexcept;
    // Returns whether logging with the given level will ultimately output something
    static bool willBeLogged(Level level) noexcept { return level >= m_current_level; }

    // a code generation macro
#define LOG_MAKE_FUNCTIONS(name, functionName, level) \
    static void name(const char *message, source_location where = source_location::current()) { \
        functionName(level, where, message); \
    } \
    template<typename T1> \
    static void name(const char *message, T1 &&arg1, source_location where = source_location::current()) { \
        functionName(level, where, message, std::forward<T1>(arg1)); \
    } \
    template<typename T1, typename T2> \
    static void name(const char *message, T1 &&arg1, T2 &&arg2, source_location where = source_location::current()) { \
        functionName(level, where, message, std::forward<T1>(arg1), std::forward<T2>(arg2)); \
    } \
    template<typename T1, typename T2, typename T3> \
    static void name(const char *message, T1 &&arg1, T2 &&arg2, T3 &&arg3, source_location where = source_location::current()) { \
        functionName(level, where, message, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3)); \
    } \
    template<typename T1, typename T2, typename T3, typename T4> \
    static void name(const char *message, T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, source_location where = source_location::current()) { \
        functionName(level, where, message, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), std::forward<T4>(arg4)); \
    } \
    template<typename T1, typename T2, typename T3, typename T4, typename T5> \
    static void name(const char *message, T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, T5 &&arg5, source_location where = source_location::current()) { \
        functionName(level, where, message, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), std::forward<T4>(arg4), std::forward<T5>(arg5)); \
    } \
    template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6> \
    static void name(const char *message, T1 &&arg1, T2 &&arg2, T3 &&arg3, T4 &&arg4, T5 &&arg5, T6 &&arg6, source_location where = source_location::current()) { \
        functionName(level, where, message, std::forward<T1>(arg1), std::forward<T2>(arg2), std::forward<T3>(arg3), std::forward<T4>(arg4), std::forward<T5>(arg5), std::forward<T6>(arg6)); \
    }

    LOG_MAKE_FUNCTIONS(debug, doDebug, Level::Debug)
    LOG_MAKE_FUNCTIONS(info, doInfo, Level::Info)
    LOG_MAKE_FUNCTIONS(warn, doWarn, Level::Warning)
    LOG_MAKE_FUNCTIONS(error, doError, Level::Error)
    LOG_MAKE_FUNCTIONS(fatal, doFatal, Level::Fatal)

#undef LOG_MAKE_FUNCTIONS

private:
    Log() = default;
    ~Log() = default;
    Log(const Log &) = delete;
    Log &operator = (const Log &) = delete;
    Log(Log &&) = delete;
    Log &operator = (Log &&) = delete;

#define DO_LOG_MAKE_FUNCTIONS(functionName, qtFunction, messageString) \
    template <class ...Args> \
    static void functionName(Level level, source_location where, const char *message, Args... args) \
    {\
        if (!willBeLogged (level))\
            return;\
        const std::string& format = kLoggingInfoFormat+message;\
        /* NOTE: LINUX ONLY */\
        const std::string_view& fullpath = std::string_view(where.file_name()); \
        const std::string_view& filename = fullpath.substr(fullpath.find_last_of("/")+1);\
        qtFunction(format.c_str(), filename.data(), where.line(), messageString, args...);\
    }

    DO_LOG_MAKE_FUNCTIONS(doDebug, qDebug, "DEBUG")
    DO_LOG_MAKE_FUNCTIONS(doInfo, qInfo, "INFO")
    DO_LOG_MAKE_FUNCTIONS(doWarn, qWarning, "WARNING")
    DO_LOG_MAKE_FUNCTIONS(doError, qCritical, "ERROR")
    DO_LOG_MAKE_FUNCTIONS(doFatal, qFatal, "FATAL")

#undef DO_LOG_MAKE_FUNCTIONS

    static const std::string kLoggingInfoFormat;
    static Level m_current_level;
};

#endif // LOGGING_H
