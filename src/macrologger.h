#ifndef __MACROLOGGER_H__
#define __MACROLOGGER_H__

#define LOG_LEVEL TRACE_LEVEL

#include <stdio.h>
#include <string.h>
#include <time.h>

static inline char *timenow();

#define _FILE strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__

#define NO_LOG          0x00
#define ERROR_LEVEL     0x01
#define WARNING_LEVEL   0x02
#define INFO_LEVEL      0x03
#define DEBUG_LEVEL     0x04
#define TRACE_LEVEL     0x05

#ifndef LOG_LEVEL
#define LOG_LEVEL   TRACE_LEVEL
#endif

#define PRINTFUNCTION(format, ...)      fprintf(stderr, format, __VA_ARGS__ )

#define LOG_FMT             "%s [%-7s] (%-10s) %15s:%-3d | "
#define LOG_ARGS(LOG_TAG)   timenow(), LOG_TAG, _FILE, __func__, __LINE__

#define ERROR_TAG   "error"
#define WARNING_TAG "warning"
#define INFO_TAG    "info"
#define DEBUG_TAG   "debug"
#define TRACE_TAG   "trace"

#define NEW_LINE "\n"

#if LOG_LEVEL >= TRACE_LEVEL
#define LOG_TRACE(message, ...) \
PRINTFUNCTION(LOG_FMT message NEW_LINE, LOG_ARGS(TRACE_TAG), __VA_ARGS__ )
#else
#define LOG_TRACE(message, ...)
#endif

#if LOG_LEVEL >= DEBUG_LEVEL
#define LOG_DEBUG(message, ...) \
PRINTFUNCTION(LOG_FMT message NEW_LINE, LOG_ARGS(DEBUG_TAG), __VA_ARGS__ )
#else
#define LOG_DEBUG(message, ...)
#endif

#if LOG_LEVEL >= INFO_LEVEL
#define LOG_INFO(message, ...) \
PRINTFUNCTION(LOG_FMT message NEW_LINE, LOG_ARGS(INFO_TAG), __VA_ARGS__ )
#else
#define LOG_INFO(message, ...)
#endif

#if LOG_LEVEL >= WARNING_LEVEL
#define LOG_WARNING(message, ...) \
PRINTFUNCTION(LOG_FMT message NEW_LINE, LOG_ARGS(WARNING_TAG), __VA_ARGS__ )
#else
#define LOG_WARNING(message, ...)
#endif

#if LOG_LEVEL >= ERROR_LEVEL
#define LOG_ERROR(message, ...) \
PRINTFUNCTION(LOG_FMT message NEW_LINE, LOG_ARGS(ERROR_TAG), __VA_ARGS__ )
#else
#define LOG_ERROR(message, ...)
#endif

#if LOG_LEVEL > NO_LOGS
#define LOG_IF_ERROR(condition, message, ...) \
if (condition) \
PRINTFUNCTION(LOG_FMT message NEW_LINE, LOG_ARGS(ERROR_TAG), __VA_ARGS__ )
#else
#define LOG_IF_ERROR(condition, message, ...)
#endif

static inline char *timenow()
{
    static char buffer[64];
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, 64, "%Y-%m-%d %H:%M:%S", timeinfo);

    return buffer;
}
 
#endif
