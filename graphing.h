

#pragma once

#include <stdint.h>

void graphStartNewGraph       (void);
void graphPlotPoint           (int channel, int32_t value);
void graphRecenter            (int channel);
void graphUpscale             (int channel, int percent);
void graphDownscale           (int channel, int percent);
void graphSetHorizSpeed       (int zoomIncrement);
int  graphGetHorizSpeed       (void);
void graphSetAvgOfFilteredData(int channel, int32_t newAverage);
void graphGetScaleAndOffset   (int32_t *numeratorPtr, int32_t *denominatorPtr, int32_t *offsetPtr);

