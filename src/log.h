#pragma once

#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#define RED_ESC     "\033[0;31m"
#define YELLOW_ESC  "\033[0;33m"
#define CYAN_ESC    "\033[0;36m"
#define RESET_ESC   "\033[m"

inline void log(const char* level, const char* tag, FILE* stream, const char* escapeCodes, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    struct timespec timespec;
    timespec_get(&timespec, TIME_UTC);

    char buf[16];
    strftime(buf, 16, "%T", gmtime(&timespec.tv_sec));

    fprintf(stream, "%s[%s.%09ld] [%s][%s]: ", escapeCodes, buf, timespec.tv_nsec, level, tag);
    vfprintf(stream, fmt, args);
    fprintf(stream, "\n" RESET_ESC);

    va_end(args);
}

#ifdef DEBUG
    #define LOG(level, tag, stream, color, ...) log(level, tag, stream, color, __VA_ARGS__)
#else
    #define LOG(level, tag, stream, color, ...)
#endif

#define LOG_ERROR(tag, ...) LOG("ERROR", tag, stderr, RED_ESC,    __VA_ARGS__)
#define LOG_WARN(tag, ...)  LOG("WARN",  tag, stderr, YELLOW_ESC, __VA_ARGS__)
#define LOG_INFO(tag, ...)  LOG("INFO",  tag, stderr, CYAN_ESC,   __VA_ARGS__)
#define LOG_DEBUG(tag, ...) LOG("DEBUG", tag, stdout, "",         __VA_ARGS__)

#include <assert.h>
#define ASSERT(predicate) assert(predicate)
#define ASSERTF(predicate, ...) do { if (!predicate) LOG_ERROR(__VA_ARGS__); ASSERT(predicate); } while(0)

#ifdef DEBUG
    #include <signal.h> // Unix-like only
    #define FORCE_BREAK raise(SIGTRAP)
#else
    #define FORCE_BREAK
#endif

#define EMPTY
#ifdef DEBUG
    #define CHECK_INTERNAL(predicate, F) do { if (!predicate) { F; FORCE_BREAK; } } while(0)
#else
    #define CHECK_INTERNAL(predicate, F)
#endif

#define CHECK(predicate) CHECK_INTERNAL(predicate, EMPTY)
#define CHECKF(predicate, ...) CHECK_INTERNAL(predicate, LOG_ERROR(__VA_ARGS__))
