/**
 * @file logger.h
 * @author Bastien Morino
 *
 * @brief Logging system & utilites.
 *
 * This file defines an interface to the logging system.
 * Log messages can be of several @ref LogLevel "priority levels".
 * Priority levels define prefixes and colors of messages, as well as their output stream.
 */

#ifndef LOGGER_H
#define LOGGER_H

enum LogLevel {
    /** Errors which prevent the server from running at all.
     * The server will always crash after raising this kind of error.
     *
     * Messages with this priority level will be sent to `stderr` by default.
     */
    LOG_LEVEL_FATAL,
    /** Errors which do not make the server crash. The server
     * will recover from this kind of error, but some parts might not work at all
     * or data might be corrupted.
     *
     * Messages with this priority level will be sent to `stderr` by default.
     */
    LOG_LEVEL_ERROR,
    /** Events that are not optimal, but the server will continue running and recover
     * without any major side effect.
     */
    LOG_LEVEL_WARN,
    /** Basic information about the running server.
     */
    LOG_LEVEL_INFO,
#ifdef DEBUG
    /** Details about the running server. Only available on debug builds.
     */
    LOG_LEVEL_DEBUG,
#endif
#ifdef TRACE
    /** Very verbose or lower level detail information.
     * Only available on debug builds with trace log level enabled.
     */
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
#define log_fatal(msg) _log_msg(LOG_LEVEL_FATAL, msg)

#define log_infof(msg, ...) _log_msgf(LOG_LEVEL_INFO, msg, __VA_ARGS__)
#define log_warnf(msg, ...) _log_msgf(LOG_LEVEL_WARN, msg, __VA_ARGS__)
#define log_errorf(msg, ...) _log_msgf(LOG_LEVEL_ERROR, msg, __VA_ARGS__)
#define log_fatalf(msg, ...) _log_msgf(LOG_LEVEL_FATAL, msg, __VA_ARGS__)

void logger_system_init(void);
void logger_system_cleanup(void);

/**
 * @brief Log a message with the given level.
 *
 *
 */
void _log_msg(enum LogLevel lvl, char* msg);
void _log_msgf(enum LogLevel lvl, char* format, ...);

#endif /* ! LOGGER_H */
