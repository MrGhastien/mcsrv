#ifndef LOGGER_H
#define LOGGER_H

enum LogLevel {
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
#ifdef DEBUG
    LOG_LEVEL_DEBUG,
#endif
#ifdef TRACE
    LOG_LEVEL_TRACE,
#endif
    _LOG_LEVEL_COUNT
};

#ifdef TRACE
#define log_trace(msg) _log_msg(LOG_LEVEL_TRACE, msg)
#define log_tracef(msg, ...) _log_msgf(LOG_LEVEL_TRACE, msg, __VA_ARGS__)
#else
#define log_trace(msg)
#define log_tracef(msg, ...)
#endif

#ifdef DEBUG
#define log_debug(msg) _log_msg(LOG_LEVEL_DEBUG, msg)
#define log_debugf(msg, ...) _log_msgf(LOG_LEVEL_DEBUG, msg, __VA_ARGS__)
#else
#define log_debug(msg)
#define log_debugf(msg, ...)
#endif

#define log_info(msg) _log_msg(LOG_LEVEL_INFO, msg)
#define log_warn(msg) _log_msg(LOG_LEVEL_WARN, msg)
#define log_error(msg) _log_msg(LOG_LEVEL_ERROR, msg)

#define log_infof(msg, ...) _log_msgf(LOG_LEVEL_INFO, msg, __VA_ARGS__)
#define log_warnf(msg, ...) _log_msgf(LOG_LEVEL_WARN, msg, __VA_ARGS__)
#define log_errorf(msg, ...) _log_msgf(LOG_LEVEL_ERROR, msg, __VA_ARGS__)

void _log_msg(enum LogLevel lvl, char* msg);
void _log_msgf(enum LogLevel lvl, char* format, ...);

#endif /* ! LOGGER_H */
