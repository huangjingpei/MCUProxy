#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <syslog.h>

#include "AudioConfMix.h"

static inline short sature32(int temp)
{
    if (temp > 32767) {
        return 32767;
    } else if (temp < -32768) {
        return -32768;
    } else {
        return (short)temp;
    }
}

void fastmix_pre_amp(fastmix_io_t *pInput,
                        int nNum, int nLen)
{
    int i, j;
    for (j=0; j<(int)nNum; ++j)
    {
        if (pInput[j].amp == MIX_DISABLE) {
            //syslog(LOG_INFO | LOG_USER, "mix-chan %d disabled", j);
            for (i=0; i<nLen; ++i)
            {
                pInput[j].pAudioData[i] = 0;
            }
        } else if (pInput[j].amp == MIX_SUPP_3dB) {
            for (i=0; i<nLen; ++i)
            {
                pInput[j].pAudioData[i] >>= 1;
            }
        } else if (pInput[j].amp == MIX_GAIN_3dB) {
            for (i=0; i<nLen; ++i)
            {
                int temp = pInput[j].pAudioData[i] * 2;
                pInput[j].pAudioData[i] = sature32(temp);
            }
        } else {
            //syslog(LOG_INFO | LOG_USER, "mix-chan %d normal", j);
        }
    }
}

void fastmix_sat_amp(int *pInput, fastmix_io_t *pOutput, int nLen)
{
    int i;
    if (pOutput->amp == MIX_DISABLE) {
        for (i=0; i<nLen; ++i)
        {
            pOutput->pAudioData[i] = 0; // mute
        }
    } else if (pOutput->amp == MIX_SUPP_3dB) {
        int Ltemp;
        for (i=0; i<nLen; ++i)
        {
            Ltemp = pInput[i] >> 1;
            pOutput->pAudioData[i] = sature32(Ltemp);
        }
    } else if (pOutput->amp == MIX_GAIN_3dB) {
        long long dtemp;
        for (i=0; i<nLen; ++i)
        {
            dtemp = pInput[i] * 2;
            pOutput->pAudioData[i] = sature32((int)dtemp);
        }
    } else { // default
        for (i=0; i<nLen; ++i)
        {
            pOutput->pAudioData[i] = sature32(pInput[i]);
        }
    }
}

void fastmix_add(fastmix_io_t *pInput, int *pOutput,
                 int nNum, int nLen,
                 int *ditherState)
{
    int i, j;

    for (i=0; i<nLen; ++i)
    {
        int Ltemp = 0;

        for (j=0; j<(int)nNum; ++j)
        {
            Ltemp += pInput[j].pAudioData[i];
        }

        //Ltemp += dither(ditherState, nNum);

        pOutput[i] = Ltemp;
    }
}

void fastmix_sub_amp(int *buffer,
                     fastmix_io_t *pInput,
                     fastmix_io_t *pOutput,
                     int nNum, int nLen)
{
    int j, i;
    long long dtemp;
    int Ltemp;

    for (j=0; j<nNum; ++j)
    {
        if (pOutput[j].amp == MIX_DISABLE) {
            for (i=0; i<nLen; ++i)
            {
                pOutput[j].pAudioData[i] = 0; // mute
            }
        } else if (pOutput[j].amp == MIX_SUPP_3dB) {
            for (i=0; i<nLen; ++i)
            {
                Ltemp = buffer[i] - pInput[j].pAudioData[i];

                pOutput[j].pAudioData[i] = sature32((Ltemp >> 1));
            }
        } else if (pOutput[j].amp == MIX_GAIN_3dB) {
            for (i=0; i<nLen; ++i)
            {
                dtemp = (buffer[i] - pInput[j].pAudioData[i]) * 2;
                pOutput[j].pAudioData[i] = sature32((int)dtemp);
            }
        } else { // default
            for (i=0; i<nLen; ++i)
            {
                Ltemp = buffer[i] - pInput[j].pAudioData[i];

                pOutput[j].pAudioData[i] = sature32(Ltemp);
            }
        }
    }
}


