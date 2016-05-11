/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file contains the DTMF tone generator and its parameters.
 *
 * A sinusoid is generated using the recursive oscillator model
 *
 *      y[n] = sin(w*n + phi) = 2*cos(w) * y[n-1] - y[n-2]
 *                            = a * y[n-1] - y[n-2]
 *
 * initialized with 
 *      y[-2] = 0
 *      y[-1] = sin(w)
 *
 * A DTMF signal is a combination of two sinusoids, depending
 * on which event is sent (i.e, which key is pressed). The following
 * table maps each key (event codes in parentheses) into two tones:
 *
 *  	    1209 Hz     1336 Hz     1477 Hz     1633 Hz
 * 697 Hz   1 (ev. 1)   2 (ev. 2) 	3 (ev. 3) 	A (ev. 12)
 * 770 Hz 	4 (ev. 4)	5 (ev. 5) 	6 (ev. 6)	B (ev. 13)
 * 852 Hz 	7 (ev. 7) 	8 (ev. 8) 	9 (ev. 9)	C (ev. 14)
 * 941 Hz 	* (ev. 10) 	0 (ev. 0)	# (ev. 11)	D (ev. 15)
 *
 * The two tones are added to form the DTMF signal.
 *
 */

#include "gentone.h"
#include <stdio.h>
#include <math.h>

//#include "signal_processing_library.h"

//#include "neteq_error_codes.h"

#define	FIX_MUL_W16(a, b) ((int)(((short)(a))*((short)(b))))
#define FIX_RSHIFT_W32(x, c) ((x)>>(c))
#define FIX_LSHIFT_W32(x, c) ((x)<<(c))


#define	DTMF_DEC_PARAMETER_ERROR	-6001
#define	DTMF_INSERT_ERROR 			-6002
#define	DTMF_GEN_UNKNOWN_SAMP_FREQ 	-6003
#define	DTMF_NOT_SUPPORTED			-6003
/*******************/
/* Constant tables */
/*******************/

/*
 * All tables corresponding to the oscillator model are organized so that
 * the coefficients for a specific frequency is found in the same position
 * in every table. The positions for the tones follow this layout:
 *
 *  dummyVector[8] =
 *  {
 *      697 Hz,	    770 Hz,	    852 Hz,     941 Hz,
 *      1209 Hz,    1336 Hz,    1477 Hz,    1633 Hz
 *  };
 */

/*
 * Tables for the constant a = 2*cos(w) = 2*cos(2*pi*f/fs) * in the oscillator model, for 8, 16, 32 and 48 kHz sample rate.  * Table values in Q14.
 */

const short dtmf_aTbl8Khz[8] =
{
    27980, 26956, 25701, 24219,
    19073, 16325, 13085, 9315
};

//#ifdef SAMP_RATE_16K
const short dtmf_aTbl16Khz[8]=
{
    31548, 31281, 30951, 30556,
    29144, 28361, 27409, 26258
};
//#endif

//#ifdef SAMP_RATE_32K
const short dtmf_aTbl32Khz[8]=
{
    32462, 32394, 32311, 32210,
    31849, 31647, 31400, 31098
};
//#endif

//#ifdef SAMP_RATE_48K
const short dtmf_aTbl48Khz[8]=
{
    32632, 32602, 32564, 32520,
    32359, 32268, 32157, 32022
};
//#endif

/*
 * Initialization values y[-1] = sin(w) = sin(2*pi*f/fs), for 8, 16, 32 and 48 kHz sample rate.
 * Table values in Q14.
 */

const short dtmf_yInitTab8Khz[8] =
{
    8528, 9315, 10163, 11036,
    13323, 14206,15021, 15708
};

//#ifdef SAMP_RATE_16K
const short dtmf_yInitTab16Khz[8]=
{
    4429, 4879, 5380, 5918,
    7490, 8207, 8979, 9801
};
//#endif

//#ifdef SAMP_RATE_32K
const short dtmf_yInitTab32Khz[8]=
{
    2235, 2468, 2728, 3010,
    3853, 4249, 4685, 5164
};
//#endif

//#ifdef SAMP_RATE_48K
const short dtmf_yInitTab48Khz[8]=
{
    1493, 1649, 1823, 2013,
    2582, 2851, 3148, 3476
};
//#endif

//dtmf  coefficients Generatef for a specific frequency 
//


static int ftoI( float f, int q)
{
    return (int)(f * (float)((1) << q)+0.5); // 四舍五入计算
}


int  get_tone_ceff(int q, int fs, int f,short *yInit,short *aTbl)
{
    float w = 0;
    float lfa, lfc;
    float fa, fc;
    int ia, ic;
   
    if (fs == 0)
    {
        fprintf(stderr, "ERROR: Sample Frequency is zero.....\n");
        return -1;
    }

    w = 2*3.14151692*f/(float)fs;
   //Init value lfa = sin(w) 
    lfa = sin(w);
   //lfc = 2 * cos(w)
    lfc = 2 * cos(w);
    *yInit = ftoI(lfa, q);
    *aTbl = ftoI(lfc, q);


    return 0;
}





/* Volume in dBm0 from 0 to -63, where 0 is the first table entry.
 Everything below -36 is discarded, wherefore the table stops at -36.
 Table entries are in Q14.
 */

const short dtfm_dBm0[37] = { 16141, 14386, 12821, 11427, 10184, 9077, 8090,
                                                7210, 6426, 5727, 5104, 4549, 4054, 3614,
                                                3221, 2870, 2558, 2280, 2032, 1811, 1614,
                                                1439, 1282, 1143, 1018, 908, 809, 721, 643,
                                                573, 510, 455, 405, 361, 322, 287, 256 };


/****************************************************************************
 * GenDtmf(...)
 *
 * Generate 10 ms DTMF signal according to input parameters.
 *
 * Input:
 *		- DTMFdecInst	: DTMF instance
 *      - value         : DTMF event number (0-15)
 *      - volume        : Volume of generated signal (0-36)
 *                        Volume is given in negative dBm0, i.e., volume == 0
 *                        means 0 dBm0 while volume == 36 mean -36 dBm0.
 *      - sampFreq      : Sample rate in Hz
 *
 * Output:
 *      - signal        : Pointer to vector where DTMF signal is stored;
 *                        Vector must be at least sampFreq/100 samples long.
 *		- DTMFdecInst	: Updated DTMF instance
 *
 * Return value			: >0 - Number of samples written to signal
 *                      : <0 - error
 */

short GenDtmf(dtmf_tone_inst_t *DTMFdecInst, char value,
                                       short volume, short *signal,
                                       short sampFreq, short extLen)
{
    const short *aTbl; /* pointer to a-coefficient table */
    const short *yInitTable; /* pointer to initialization value table */
    short a1 = 0; /* a-coefficient for first tone (low tone) */
    short a2 = 0; /* a-coefficient for second tone (high tone) */
    int i;
    int frameLen; /* number of samples to generate */
    int lowIndex = 0;  /* Default to avoid compiler warnings. */
    int highIndex = 4;  /* Default to avoid compiler warnings. */
    int tempVal;
    short tempValLow;
    short tempValHigh;
	int	tmpl, tmph;
    if ((volume < 0) || (volume > 36))
    {
        return DTMF_DEC_PARAMETER_ERROR;
    }

    /* Sanity check for extLen */
    if (extLen < -1)
    {
        return DTMF_DEC_PARAMETER_ERROR;
    }

    if (sampFreq == 8000)
    {
        aTbl = dtmf_aTbl8Khz;
        yInitTable = dtmf_yInitTab8Khz;
        frameLen = 80;
    }
    else if (sampFreq == 16000)
    {
        aTbl = dtmf_aTbl16Khz;
        yInitTable = dtmf_yInitTab16Khz;
        frameLen = 160;
    }
    else if (sampFreq == 32000)
    {
        aTbl = dtmf_aTbl32Khz;
        yInitTable = dtmf_yInitTab32Khz;
        frameLen = 320;
    }
    else if (sampFreq == 48000)
    {
        aTbl = dtmf_aTbl48Khz;
        yInitTable = dtmf_yInitTab48Khz;
        frameLen = 480;
    }
    else
    {
        /* unsupported sample rate */
        return DTMF_GEN_UNKNOWN_SAMP_FREQ;
    }

    if (extLen >= 0)
    {
        frameLen = extLen;
    }

    /* select low frequency based on event value */
    switch (value)
    {
        case '1':
        case '2':
        case '3':
        case 'a': /* first row on keypad */
        {
            lowIndex = 0; /* low frequency: 697 Hz */
            break;
        }
        case '4':
        case '5':
        case '6':
        case 'b': /* second row on keypad */
        {
            lowIndex = 1; /* low frequency: 770 Hz */
            break;
        }
        case '7':
        case '8':
        case '9':
        case 'c': /* third row on keypad */
        {
            lowIndex = 2; /* low frequency: 852 Hz */
            break;
        }
        case '*':
        case '0':
        case '#':
        case 'd': /* fourth row on keypad */
        {
            lowIndex = 3; /* low frequency: 941 Hz */
            break;
        }
        default:
        {
            return DTMF_DEC_PARAMETER_ERROR;
        }
    } /* end switch */

    /* select high frequency based on event value */
    switch (value)
    {
        case '1':
        case '4':
        case '7':
        case '*': /* first column on keypad */
        {
            highIndex = 4; /* high frequency: 1209 Hz */
            break;
        }
        case '2':
        case '5':
        case '8':
        case '0': /* second column on keypad */
        {
            highIndex = 5;/* high frequency: 1336 Hz */
            break;
        }
        case '3':
        case '6':
        case '9':
        case '#': /* third column on keypad */
        {
            highIndex = 6;/* high frequency: 1477 Hz */
            break;
        }
        case 'a':
        case 'b':
        case 'c':
        case 'd': /* fourth column on keypad (special) */
        {
            highIndex = 7;/* high frequency: 1633 Hz */
            break;
        }
    } /* end switch */

    /* select coefficients based on results from switches above */
    a1 = aTbl[lowIndex]; /* coefficient for first (low) tone */
    a2 = aTbl[highIndex]; /* coefficient for second (high) tone */

    if (DTMFdecInst->reinit)
    {
		//printf("<DTMF>: Re-init !!!!!!!!!!!!!!!");
        /* set initial values for the recursive model */
        DTMFdecInst->oldOutputLow[0] = yInitTable[lowIndex];
        DTMFdecInst->oldOutputLow[1] = 0;
        DTMFdecInst->oldOutputHigh[0] = yInitTable[highIndex];
        DTMFdecInst->oldOutputHigh[1] = 0;

        /* reset reinit flag */
        DTMFdecInst->reinit = 0;
    }
	
//	printf("<DTMF>: %d, %d", a1, a2);
    /* generate signal sample by sample */
    for (i = 0; i < frameLen; i++)
    {
        /* Use rescursion formula y[n] = a*y[n-1] - y[n-2] */
        tempValLow = (short) (((FIX_MUL_W16(a1, DTMFdecInst->oldOutputLow[1])
                                        + 8192) >> 14) - DTMFdecInst->oldOutputLow[0]);
        tempValHigh = (short) (((FIX_MUL_W16(a2, DTMFdecInst->oldOutputHigh[1])
                                        + 8192) >> 14) - DTMFdecInst->oldOutputHigh[0]);

        /* Update recursion memory */
        DTMFdecInst->oldOutputLow[0] = DTMFdecInst->oldOutputLow[1];
        DTMFdecInst->oldOutputLow[1] = tempValLow;
        DTMFdecInst->oldOutputHigh[0] = DTMFdecInst->oldOutputHigh[1];
        DTMFdecInst->oldOutputHigh[1] = tempValHigh;

        /* scale high tone with 32768 (15 left shifts) 
         and low tone with 23171 (3dB lower than high tone) */
        tempVal = FIX_MUL_W16(DTMF_AMP_LOW, tempValLow)
                       + FIX_LSHIFT_W32((int)tempValHigh, 15);

        /* Norm the signal to Q14 (with proper rounding) */
        tempVal = (tempVal + 16384) >> 15;

        /* Scale the signal to correct dbM0 value */
        signal[i] = (short) FIX_RSHIFT_W32(
                              (FIX_MUL_W16(tempVal, dtfm_dBm0[volume])
                               + 8192), 14); /* volume value is in Q14; use proper rounding */
    }

    return frameLen;
}

short GenTone(dtmf_tone_inst_t *DTMFdecInst, short *yInit, short *aTbl,
                                       short volume, short *signal,
                                       short sampFreq, short extLen)
{
    short a1 = 0; /* a-coefficient for first tone (low tone) */
    short a2 = 0; /* a-coefficient for second tone (high tone) */
    int i;
    int frameLen; /* number of samples to generate */
    int tempVal;
    short tempValLow;
    short tempValHigh;
	int	tmpl, tmph;
    if ((volume < 0) || (volume > 36))
    {
        return  DTMF_DEC_PARAMETER_ERROR;
    }

    /* Sanity check for extLen */
    if (extLen < -1)
    {
        return DTMF_DEC_PARAMETER_ERROR;
    }

    if (extLen >= 0)
    {
        frameLen = extLen;
    }

   /* select coefficients based on results from switches above */
    a1 = aTbl[0]; /* coefficient for first (low) tone */
    a2 = aTbl[1]; /* coefficient for second (high) tone */

    if (DTMFdecInst->reinit)
    {
		//printf("<DTMF>: Re-init !!!!!!!!!!!!!!!");
        /* set initial values for the recursive model */
        DTMFdecInst->oldOutputLow[0] = yInit[0];
        DTMFdecInst->oldOutputLow[1] = 0;
        DTMFdecInst->oldOutputHigh[0] = yInit[1];
        DTMFdecInst->oldOutputHigh[1] = 0;

        /* reset reinit flag */
        DTMFdecInst->reinit = 0;
    }
	
//	printf("<DTMF>: %d, %d", a1, a2);
    /* generate signal sample by sample */
    for (i = 0; i < frameLen; i++)
    {
        /* Use rescursion formula y[n] = a*y[n-1] - y[n-2] */
        tempValLow = (short) (((FIX_MUL_W16(a1, DTMFdecInst->oldOutputLow[1])
                                        + 8192) >> 14) - DTMFdecInst->oldOutputLow[0]);
        tempValHigh = (short) (((FIX_MUL_W16(a2, DTMFdecInst->oldOutputHigh[1])
                                        + 8192) >> 14) - DTMFdecInst->oldOutputHigh[0]);

        /* Update recursion memory */
        DTMFdecInst->oldOutputLow[0] = DTMFdecInst->oldOutputLow[1];
        DTMFdecInst->oldOutputLow[1] = tempValLow;
        DTMFdecInst->oldOutputHigh[0] = DTMFdecInst->oldOutputHigh[1];
        DTMFdecInst->oldOutputHigh[1] = tempValHigh;

        /* scale high tone with 32768 (15 left shifts) 
         and low tone with 23171 (3dB lower than high tone) */
        tempVal = FIX_MUL_W16(DTMF_AMP_LOW, tempValLow)
                       + FIX_LSHIFT_W32((int)tempValHigh, 15);

        /* Norm the signal to Q14 (with proper rounding) */
        tempVal = (tempVal + 16384) >> 15;

        /* Scale the signal to correct dbM0 value */
        signal[i] = (short) FIX_RSHIFT_W32(
                              (FIX_MUL_W16(tempVal, dtfm_dBm0[volume])
                               + 8192), 14); /* volume value is in Q14; use proper rounding */
    }

    return frameLen;
}
