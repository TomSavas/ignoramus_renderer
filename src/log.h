#pragma once

#include <stdio.h>
#include <time.h>
#include <stdarg.h>

inline void log(const char* tag, FILE* stream, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    struct timespec timespec;
    timespec_get(&timespec, TIME_UTC);

    char buf[16];
    strftime(buf, 16, "%T", gmtime(&timespec.tv_sec));

    fprintf(stream, "[%s.%09ld] [%s]: ", buf, timespec.tv_nsec, tag);
    vfprintf(stream, fmt, args);
    fprintf(stream, "\n");

    va_end(args);
}

#define LOG_TAG(tag, stream, ...) log(#tag, stream, __VA_ARGS__)

#define LOG_INFO(...)       LOG_TAG(INFO,  stderr, __VA_ARGS__)
#define LOG_WARN(...)       LOG_TAG(WARN,  stderr, __VA_ARGS__)
#define LOG_ERROR(...)      LOG_TAG(ERROR, stderr, __VA_ARGS__)
#ifdef DEBUG
    #define LOG_DEBUG(...)  LOG_TAG(DEBUG, stdout, __VA_ARGS__)
#else
    #define LOG_DEBUG(...)
#endif

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
