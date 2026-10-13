#ifndef LOGGING_H
#define LOGGING_H

#include "genesis.h"
#include <conf/logging_conf.h>

#define LOGGING_WRITER_KPRINT 0
#define LOGGING_WRITER_CONSOLE 1

#if LOGGING_WRITER == LOGGING_WRITER_KPRINT
#define LOGGING_WRITER_NG(fmt, ...) kprintf(fmt, __VA_ARGS__)
#elif LOGGING_WRITER == LOGGING_WRITER_CONSOLE
#define LOGGING_WRITER_NG(fmt, ...) CON_write(fmt, __VA_ARGS__)
#endif

#ifdef DEBUG
#if (defined LOG_LEVEL_FILE  && LOG_LEVEL_FILE>= LOG_LEVEL_TRACE) || LOG_LEVEL >= LOG_LEVEL_TRACE
#define LOG_TRACE(fmt, ...) LOGGING_print("TRACE", __func__, fmt, ##__VA_ARGS__)
#define LOG_TRACE_MATRIX(fmt, mtx, w, size) LOGGING_printMatrix("TRACE", __func__, fmt, mtx, w, size)
#else
#define LOG_TRACE(fmt, ...)
#define LOG_TRACE_MATRIX(fmt, mtx, w, size)
#endif

#if (defined LOG_LEVEL_FILE  && LOG_LEVEL_FILE>= LOG_LEVEL_DEBUG) || LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...) LOGGING_print("DEBUG", __func__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG_MATRIX(fmt, mtx, w, size) LOGGING_printMatrix("DEBUG", __func__, fmt, mtx, w, size)
#else
#define LOG_DEBUG(fmt, ...)
#define LOG_DEBUG_MATRIX(fmt, mtx, w, size)
#endif

#if (defined LOG_LEVEL_FILE && LOG_LEVEL_FILE>= LOG_LEVEL_INFO) || LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO(fmt, ...) LOGGING_print("INFO", __func__, fmt, ##__VA_ARGS__)
#define LOG_INFO_MATRIX(fmt, mtx, w, size) LOGGING_printMatrix("INFO", __func__, fmt, mtx, w, size)
#else
#define LOG_INFO(fmt, ...)
#define LOG_INFO_MATRIX(fmt, mtx, w, size)
#endif

#if (defined LOG_LEVEL_FILE && LOG_LEVEL_FILE>= LOG_LEVEL_WARN) || LOG_LEVEL >= LOG_LEVEL_WARN
#define LOG_WARN(fmt, ...) LOGGING_print("WARN", __func__, fmt, ##__VA_ARGS__)
#define LOG_WARN_MATRIX(fmt, mtx, w, size) LOGGING_printMatrix("WARN", __func__, fmt, mtx, w, size)
#else
#define LOG_WARN(fmt, ...)
#define LOG_WARN_MATRIX(fmt, mtx, w, size)
#endif

#if (defined LOG_LEVEL_FILE && LOG_LEVEL_FILE>= LOG_LEVEL_ERROR) || LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_ERROR(fmt, ...) LOGGING_print("ERROR", __func__, fmt, ##__VA_ARGS__)
#define LOG_ERROR_MATRIX(fmt, mtx, w, size) LOGGING_printMatrix("ERROR", __func__, fmt, mtx, w, size)
#else
#define LOG_ERROR(fmt, ...)
#define LOG_ERROR_MATRIX(fmt, mtx, w, size)
#endif
#else /* !DEBUG */
#define LOG_TRACE(fmt, ...)
#define LOG_TRACE_MATRIX(fmt, mtx, w, size)
#define LOG_DEBUG(fmt, ...)
#define LOG_DEBUG_MATRIX(fmt, mtx, w, size)
#define LOG_INFO(fmt, ...)
#define LOG_INFO_MATRIX(fmt, mtx, w, size)
#define LOG_WARN(fmt, ...)
#define LOG_WARN_MATRIX(fmt, mtx, w, size)
#define LOG_ERROR(fmt, ...)
#define LOG_ERROR_MATRIX(fmt, mtx, w, size)
#endif /* DEBUG */

#if LOGGING_ENABLE_BINARY_FORMAT
#define BINMARK "%b"
#else
#define BINMARK "%x"
#endif

void LOGGING_print(const char *level, const char *fun, const char *fmt, ...);
void LOGGING_printMatrix(const char *level, const char *fun, const char *fmt, void *mtx, u16 w, u16 size);

#endif // LOGGING_H