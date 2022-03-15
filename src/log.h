#pragma once

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

inline char* annotateLineNumbers(const char* str)
{
    int lineCount = 0;
    const char* lastNewline = str;
    while (lastNewline != NULL && (lastNewline + 1) != NULL)
    {
        lineCount++;
        lastNewline = strstr(lastNewline + 1, "\n");
    }
    lastNewline = str;

    int maxLineMagnitude = log10(lineCount);
    char* annotatedStr = (char*) malloc(sizeof(char) * (strlen(str) + lineCount * (maxLineMagnitude + 2 + 3) + 1));
    annotatedStr[0] = '\0';

    char annotationFormat[16];
    sprintf(annotationFormat, "%%%dd| ", maxLineMagnitude + 2);

    for (int i = 1; i <= lineCount; i++)
    {
        // Stupid, but assume that there won't be more than 10^9 lines
        char lineAnnotation[10];
        sprintf(lineAnnotation, annotationFormat, i);
        
        if (lastNewline == NULL || (lastNewline + 1) == NULL)
            break;
        const char* nextNewline = strstr(lastNewline + 1, "\n");
        strcat(annotatedStr, lineAnnotation);
        strncat(annotatedStr, lastNewline + 1, nextNewline - lastNewline);
        lastNewline = nextNewline;
    }

    return annotatedStr;
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
#define ASSERTF(predicate, ...) do { if (!(predicate)) { LOG_ERROR(__VA_ARGS__); } ASSERT(predicate); } while(0)

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
