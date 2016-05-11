#ifndef TONE_GENERATOR_H
#define TONE_GENERATOR_H

#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <utils/threads.h>
#include <utils/List.h>
#include <list>

#include "gentone.h"
#include "defines.h"

#define DT_BLK_NUM  32
#define RND_DT_PT(x)    ((x)&= (DT_BLK_NUM-1))

using namespace android;
using std::list;

class DtmfGene : public RefBase
{
public:
    DtmfGene();
    virtual ~DtmfGene() {};

    status_t SetDtmf(char keyVal);

    //status_t updateFrame(AudioFrame &_frame, bool inAudio);
    status_t updateFrame(int16_t *frame, uint32_t nSamples, uint16_t value, int16_t inAudio);
private:

    int gen(int16_t inAudio, uint16_t value);

    int dtVal;
    dtmf_tone_inst_t dtu[10];
    short   dtmfBuf[DT_BLK_NUM*EVENT_SAMPLES];

};

class ToneGene : public RefBase
{
public:
    static inline sp<ToneGene> Instance()
    {
        if (instance == NULL) {
            Mutex::Autolock _l(cLock);
            do {
                if (instance != NULL)
                    break;

                sp<ToneGene> ptr = new ToneGene();
                if (ptr == NULL)
                    break;

                instance = ptr;
            }while(0);
        }

        return instance;
    };


    // tone
    status_t setToneScript(char *_tones);
    /// do not set tx tone here, it is negotiated by APM and AudioConfMix

    bool empty();

    //status_t updateFrame(AudioFrame &_frame, bool refresh);
    status_t updateFrame(int16_t *frame, uint32_t nSamples,uint16_t value,  bool refresh);

private:
    typedef struct {
        uint32_t freq1;
        uint32_t freq2;

        int64_t timeOn;
        int64_t timeOff;
    }Cadence;

    //status_t setCadence(Cadence cad);
    //status_t setCadence(list<Cadence> cadList);

    Mutex mLock;
    list<Cadence> mToneList;
    Cadence *cadNow;

    short yInit[2];
    short aTbl[2];
    short freq1;
    short freq2;

    dtmf_tone_inst_t dtmfdec;

    bool toneGen_INF;
    bool toneInTx;
    short toneBuf[EVENT_SAMPLES];

    static Mutex cLock;
    static sp<ToneGene> instance;
private:
    ToneGene();
    ToneGene(const ToneGene&);
    void operator=(const ToneGene&);
};

#endif

