#include "perf_data.h"

#include "log.h"

float RemoveFromRollingAverage(float rollingAverage, int countAfterRemoving, float valueToRemove)
{
    int countBeforeRemoving = countAfterRemoving + 1;
    return countAfterRemoving <= 0
        ? 0.f 
        : ((float)countBeforeRemoving * rollingAverage - valueToRemove) / (float)countAfterRemoving;
}

float AddToRollingAverage(float rollingAverage, int countAfterAdding, float valueToAdd)
{
    return rollingAverage + (valueToAdd - rollingAverage) / (float)countAfterAdding;
}

float ReplaceInRollingAverage(float rollingAverage, int count, float replacedValue, float replacingValue)
{
    return AddToRollingAverage(RemoveFromRollingAverage(rollingAverage, count-1, replacedValue), count, replacingValue);
}

void FrametimePerfData::AddFrametime(float frametime)
{
    lifetimeMinFrametime = lifetimeMinFrametime < frametime ? lifetimeMinFrametime : frametime;
    lifetimeMaxFrametime = lifetimeMaxFrametime > frametime ? lifetimeMaxFrametime : frametime;
    lifetimeAvgFrametime = AddToRollingAverage(lifetimeAvgFrametime, ++lifetimeDatapointCount, frametime);

    float removedValue = data[latestIndex];
    int datapointCount = PERF_DATAPOINT_COUNT;
    if (!filledBufferOnce)
    {
        removedValue = avgFrametime; // causes RemoveFromRollingAverage to have no effect
        datapointCount = latestIndex+1;
        filledBufferOnce = latestIndex == PERF_DATAPOINT_COUNT-1;
    }
    //avgFrametime = ReplaceInRollingAverage(avgFrametime, datapointCount, removedValue, frametime);

    latestIndex = (latestIndex+1) % PERF_DATAPOINT_COUNT; 
    data[latestIndex] = frametime;

    float sum = 0.f;
    minFrametime = frametime;
    maxFrametime = frametime;
    for (int i = 0; i < datapointCount; i++)
    {
        sum += data[i];
        minFrametime = minFrametime < data[i] ? minFrametime : data[i];
        maxFrametime = maxFrametime > data[i] ? maxFrametime : data[i];
    }
    avgFrametime = sum / (float)PERF_DATAPOINT_COUNT;
}
