#pragma once

#define PERF_DATAPOINT_COUNT 256

struct FrametimePerfData
{
    float lifetimeMinFrametime = 10000000.f;
    float lifetimeMaxFrametime = 0.f;
    float lifetimeAvgFrametime = 0.f;
    unsigned long lifetimeDatapointCount = 0;

    float minFrametime = 1000000.f;
    float maxFrametime = 0.f;
    float avgFrametime = 0.f;

    bool filledBufferOnce = false;
    int latestIndex = 0;
    float data[PERF_DATAPOINT_COUNT];

    void AddFrametime(float frametime);
};

struct PerfData
{
    FrametimePerfData cpu;
    FrametimePerfData gpu;
};
