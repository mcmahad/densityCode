
#ifndef    _WIN32
#include <arduino.h>
#endif // !_WIN32

#include "commonHeader.h"

#include <stdint.h>
#include <stdbool.h>
#include "graphing.h"


#define MAX_GRAPH_CHANNELS        4
#define GRAPH_VERTICALPIXEL_CNT 200

#ifdef _WIN32
#include "win32shims.h"
static HardwareSerial  dbgSerial;
static HardwareSerial  nextionSerial;
#else
extern HardwareSerial &dbgSerial;
extern HardwareSerial &nextionSerial;
#endif // _WIN32

static void graphRescale(int channel);

static  int32_t graphAveragedFilteredValue[MAX_GRAPH_CHANNELS] = { 0 },
                graphOffset[MAX_GRAPH_CHANNELS]                = { 0 },
                graphScaleNumerator[MAX_GRAPH_CHANNELS]        = { 100 },
                graphScaleDenominator[MAX_GRAPH_CHANNELS]      = { 100 };


static  bool    useNextToReCenter[MAX_GRAPH_CHANNELS] = { false };
static  int8_t  zoomScrollingSpeed = 1;


void graphPlotPoint(int channel, int32_t value)
{
    /*  value is a 24-bit signed value coming from ADC

        Apply offset and scaling.  Offset first.
    */
    int   watchChannel = -99;

    if (useNextToReCenter[channel]  &&  channel > 0)
    {
        graphOffset      [channel] = value - graphAveragedFilteredValue[channel];
        useNextToReCenter[channel] = false;

//      dbgSerial.println(F("Set one recenter point"));

        if (channel == 1)   //  Update the raw offset
        {
            graphOffset      [0] = value - graphAveragedFilteredValue[channel];
            useNextToReCenter[0] = false;
//          dbgSerial.println(F("Set two recenter points"));
        }

        if (channel == watchChannel)
        {
            dbgSerial.print(F("Chan"));
            dbgSerial.print(channel);
            dbgSerial.print(F("  val="));
            dbgSerial.print(value);
            dbgSerial.print(F("  avgFilt="));
            dbgSerial.print(graphAveragedFilteredValue[channel]);
            dbgSerial.print(F("  newOffset="));
            dbgSerial.print(graphOffset[channel]);
            dbgSerial.println();
        }
    }

    if (channel < 0  || channel >= MAX_GRAPH_CHANNELS) channel = 0;

    if (channel == watchChannel)
    {
        dbgSerial.print(F("Chan"));
        dbgSerial.print(channel);
        dbgSerial.print(F("  val="));
        dbgSerial.print(value);
        dbgSerial.print(F("  avgFilt="));
        dbgSerial.print(graphAveragedFilteredValue[channel]);
        dbgSerial.print(F("  offset="));
        dbgSerial.print(graphOffset[channel]);
        dbgSerial.println();
    }

    //                        New           Centerline offset                  User zero
    int32_t     valueToPlot = value - graphAveragedFilteredValue[channel] - graphOffset[channel];

    if (channel == watchChannel)
    {
        dbgSerial.print(F("val0="));
        dbgSerial.print(value);
        dbgSerial.print(F(" avgFilt="));
        dbgSerial.print(graphAveragedFilteredValue[channel]);
        dbgSerial.print(F(" Offset="));
        dbgSerial.print(graphOffset[channel]);

        dbgSerial.print(F("val1="));
        dbgSerial.print(valueToPlot);
    }

    if (channel > 0)
    {   //  Don't scale the tare indicator
        valueToPlot *= graphScaleNumerator  [channel];
        valueToPlot /= graphScaleDenominator[channel];
    }

    if (channel == watchChannel)
    {
        dbgSerial.print(F("  val2="));
        dbgSerial.print(valueToPlot);
    }

    /*  ValueToPlot should now be somewhere in the range of +/-100.  */

    /*  Offset the point to be in the center of the graph  */
    valueToPlot += GRAPH_VERTICALPIXEL_CNT / 2;

    if (channel == watchChannel)
    {
        dbgSerial.print(F("  val3="));
        dbgSerial.print(valueToPlot);
        dbgSerial.println();
    }

    /*  ValueToPlot should now be somewhere in the range of 0 to 200 */

    /*  Put limits on the point to avoid boundaries */
    valueToPlot = (valueToPlot < 1) ? 1 : valueToPlot;
    valueToPlot = (valueToPlot > GRAPH_VERTICALPIXEL_CNT - 2) ? GRAPH_VERTICALPIXEL_CNT - 2 : valueToPlot;

    if (1)
    {
        static  int     lastValueToPlot[4] = { 0 };
        int     delta = valueToPlot - lastValueToPlot[channel];

        /*  Send out the point to Nextion  */
        for (int zoomIndex = 1; zoomIndex < zoomScrollingSpeed - 1; zoomIndex++)
        {
            //  Fill in the intermediate points for faster scrolling speeds to draw straight lines and not stairsteps
            nextionSerial.print(F("add 15,"));
            nextionSerial.print(channel);
            nextionSerial.print(F(","));
            nextionSerial.print(lastValueToPlot[channel] + (delta * zoomIndex) / zoomScrollingSpeed);
            nextionSerial.print(F("\xFF\xFF\xFF"));
            lastValueToPlot[channel] = valueToPlot;
        }
        nextionSerial.print(F("add 15,"));
        nextionSerial.print(channel);
        nextionSerial.print(F(","));
        nextionSerial.print(valueToPlot);
        nextionSerial.print(F("\xFF\xFF\xFF"));
        lastValueToPlot[channel] = valueToPlot;
    }
}

void graphSetHorizSpeed(int zoomIncrement)
{
    zoomScrollingSpeed += zoomIncrement;
    if (zoomIncrement == 0) zoomScrollingSpeed = 1;

    if (zoomScrollingSpeed <  1) zoomScrollingSpeed =  1;
    if (zoomScrollingSpeed > 10) zoomScrollingSpeed = 10;
}


int  graphGetHorizSpeed(void)
{
    return zoomScrollingSpeed;
}


void graphStartNewGraph(void)
{
    for (int index = 0; index < MAX_GRAPH_CHANNELS; index++)
    {
        graphOffset[index]                 = 0;
        graphScaleNumerator[index]         = 100;
        graphScaleDenominator[index]       = 100;
        graphAveragedFilteredValue[index]  = 0;
        graphRescale(index);

        //  Erase any graphed points and start fresh
        nextionSerial.print(F("cle 15,"));
        nextionSerial.print(index);
        nextionSerial.print(F("\xFF\xFF\xFF"));
    }
    zoomScrollingSpeed = 1;
}

void graphSetAvgOfFilteredData(int channel, int32_t newAverage)
{
    graphAveragedFilteredValue[channel] = newAverage;
}

void graphGetScaleAndOffset(int32_t *numeratorPtr, int32_t *denominatorPtr, int32_t *offsetPtr)
{
    if (numeratorPtr)
    {
        *numeratorPtr = graphScaleNumerator[0];
    }

    if (denominatorPtr)
    {
        *denominatorPtr = graphScaleDenominator[0];
    }

    if (offsetPtr)
    {
        *offsetPtr = graphOffset[0];
    }
}

void graphRecenter(int channel)
{
    /*  Select the next point to be the graph center
    */
    useNextToReCenter[channel] = true;
}


static void graphRescale(int channel)
{
    while (graphScaleNumerator[channel] > 10000000000 ||  graphScaleDenominator[channel] > 10000000000)
    {
        graphScaleNumerator  [channel] /= 2;
        graphScaleDenominator[channel] /= 2;
    }

    {
        nextionSerial.print(F("ratio.val="));
        nextionSerial.print((int32_t)((float)graphScaleNumerator[channel] * 1000.0 / (float)graphScaleDenominator[channel]));
        nextionSerial.print(F("\xFF\xFF\xFF"));
    }
}


void graphUpscale(int channel, int percent)
{
    graphScaleNumerator[channel] *= (100 + percent);
    graphScaleNumerator[channel] /= 100;

    while ( graphScaleDenominator[channel] % 2 == 0  &&
            graphScaleNumerator  [channel] % 2 == 0  &&
            graphScaleDenominator[channel] > 100     &&
            graphScaleNumerator  [channel] > 100 )
    {
        graphScaleDenominator[channel] /= 2;
        graphScaleNumerator  [channel] /= 2;
    }

    while ( graphScaleDenominator[channel] % 5 == 0  &&
            graphScaleNumerator  [channel] % 5 == 0  &&
            graphScaleDenominator[channel] > 100     &&
            graphScaleNumerator  [channel] > 100 )
    {
        graphScaleDenominator[channel] /= 5;
        graphScaleNumerator  [channel] /= 5;
    }

    graphRescale(channel);

    int   watchChannel = 3;

    if (channel == watchChannel)
    {
        dbgSerial.print(F("graphUpscale()  "));
        dbgSerial.print(channel);
        dbgSerial.print(F(" "));
        dbgSerial.print(percent);
        dbgSerial.print(F("%  "));
        dbgSerial.print(graphScaleNumerator[channel]);
        dbgSerial.println();
    }
}


void graphDownscale(int channel, int percent)
{
    int     watchChannel = 3;

    if (channel == watchChannel)
    {
        dbgSerial.print(F("graphDownscale#:  "));
        dbgSerial.print(graphScaleDenominator[channel]);
        dbgSerial.print(F("  "));
    }
    graphScaleDenominator[channel] *= percent;
    graphScaleDenominator[channel] /= 100;
    if (channel == watchChannel)
    {
        dbgSerial.print(graphScaleDenominator[channel]);
        dbgSerial.print(F("  "));
    }

    while ( graphScaleDenominator[channel] % 2 == 0  &&
            graphScaleNumerator  [channel] % 2 == 0  &&
            graphScaleDenominator[channel] > 100     &&
            graphScaleNumerator  [channel] > 100 )
    {
        graphScaleDenominator[channel] /= 2;
        graphScaleNumerator  [channel] /= 2;
    }

    while ( graphScaleDenominator[channel] % 5 == 0  &&
            graphScaleNumerator  [channel] % 5 == 0  &&
            graphScaleDenominator[channel] > 100     &&
            graphScaleNumerator  [channel] > 100 )
    {
        graphScaleDenominator[channel] /= 5;
        graphScaleNumerator  [channel] /= 5;
    }

    graphRescale(channel);
    if (channel == watchChannel)
    {
        dbgSerial.print(graphScaleDenominator[channel]);
        dbgSerial.print(F("\n"));

        dbgSerial.print(F("graphDownscale()  "));
        dbgSerial.print(channel);
        dbgSerial.print(F(" "));
        dbgSerial.print(percent);
        dbgSerial.print(F("%  "));
        dbgSerial.print(graphScaleDenominator[channel]);
        dbgSerial.println();
    }
}
