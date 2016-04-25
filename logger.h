#ifndef LOGGER_GLIB
#define LOGGER_GLIB
#include <glib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

void init_logger();

void _log_wrap(GLogLevelFlags level, const char* fmt, ...);

#define LOG(level, msg, ...) _log_wrap(level, "[" LOG_TAG "] " msg, ##__VA_ARGS__)
#define D(msg, ...) LOG(G_LOG_LEVEL_DEBUG, msg, ##__VA_ARGS__)
#define E(msg, ...) LOG(G_LOG_LEVEL_CRITICAL, msg, ##__VA_ARGS__)
#define ERR(msg, ...) LOG(G_LOG_LEVEL_ERROR, msg, ##__VA_ARGS__)
#define W(msg, ...) LOG(G_LOG_LEVEL_WARNING, msg, ##__VA_ARGS__)
#define I(msg, ...) LOG(G_LOG_LEVEL_INFO, msg, ##__VA_ARGS__)
#define C(msg, ...) LOG(G_LOG_LEVEL_CRITICAL, msg, ##__VA_ARGS__)

#endif
