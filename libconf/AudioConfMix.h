#ifndef AUDIO_CONFERENCE_MIXER_H
#define AUDIO_CONFERENCE_MIXER_H


#define MAX_VOIP_OBJECT     (6)
#define MAX_MIX_OBJECT      (8)

typedef enum {
    MIX_DISABLE = -1,
    MIX_KEEP_0dB = 0, /* 0dB */
    MIX_SUPP_3dB = 1, /* -3dB */
    MIX_GAIN_3dB = 3  /* +3dB */
}fastmix_vol_t;

typedef struct {
    fastmix_vol_t amp;
    short *pAudioData;
}fastmix_io_t;

#if defined(__cplusplus)

#include <utils/String8.h>
#include <utils/KeyedVector.h>
#include <utils/threads.h>
#include <utils/Vector.h>
#include <list>
#include <algorithm>

#include "IMultiParty.h"

#include "ToneGene.h"

using namespace android;
using std::list;

class AudioConfMix : public IConfMix
{
public:
    virtual ~AudioConfMix();

    static AudioConfMix *create();

    virtual status_t start();
    virtual status_t reset();

    status_t setParameters(const KeyedVector<String8, String8>& params,
                           uint32_t mid);

    // IMultiParty interface
    virtual status_t doInsertNode(IMediaNode *node);
    virtual status_t doRemoveNode(IMediaNode *node);
    virtual void dumpInfo(void *stream);

private:
    struct mixMeta
    {   // pre-allocate buffer
        int16_t txData[EVENT_SAMPLES];
        int16_t rxData[EVENT_SAMPLES];

        sp<MetaBuffer> txBuffer;
        sp<MetaBuffer> rxBuffer;
    };

    status_t initMixer();

    status_t mix(fastmix_io_t *pInput,
                 fastmix_io_t *pFull, fastmix_io_t *pInter);

    // Thread interface
    virtual bool threadLoop();
    bool mstate;
    static unsigned int mSourceNum;
    static unsigned int mSampleLen;


    Mutex mMixLock;
    Condition mAvailable;

    list<IMediaNode* >mNodeList;

    mixMeta mMetas[MAX_MIX_OBJECT];
    int mMixNum;

    fastmix_io_t AudioIn[MAX_MIX_OBJECT];
    fastmix_io_t AudioOut[MAX_MIX_OBJECT];

    int *buffer;

    AudioConfMix();
    AudioConfMix(const AudioConfMix&);
    void operator=(const AudioConfMix&);
};

#endif // __cplusplus

#endif
