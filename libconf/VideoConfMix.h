#ifndef VIDEO_CONFERENCE_MIXER_H
#define VIDEO_CONFERENCE_MIXER_H

#include <utils/String8.h>
#include <utils/KeyedVector.h>
#include <utils/threads.h>
#include <utils/List.h>
///#include <list>
#include <semaphore.h>

#include "IMultiParty.h"

#define MAX_VIDEO_CLIENT    (2)
#define MAX_VIDEO_CAMERA    (1)
#define MAX_VIDEO_ENCODER   (1)
#define MAX_VIDEO_ITEMS     (MAX_VIDEO_CAMERA+MAX_VIDEO_CLIENT+MAX_VIDEO_ENCODER)

using namespace android;
///using std::list;

class VideoConfMix : public IConfMix
{
public:
    virtual ~VideoConfMix();

    static VideoConfMix *create();

    virtual status_t start();
    virtual status_t reset();

    status_t setParameters(KeyedVector<String8, String8>& params);


    // IMultiParty interface
    virtual void dumpInfo(void *stream);

private:

    struct mixMeta : public RefBase
    {   // no allocate
        sp<MetaBuffer> txBuffer;
        sp<MetaBuffer> rxBuffer;

        bool inConf;
        KeyedVector<String8, String8> param;
    };

    status_t init();

    //status_t setupMedia(uint32_t mid=0);
    //status_t setPropParams(const char *sprop_param, uint32_t mid=0); // for H.264

    // Thread
    virtual bool threadLoop();

    // IMultiParty interface
    virtual status_t doInsertNode(IMediaNode *node);
    virtual status_t doRemoveNode(IMediaNode *node);

    Mutex mLockQ; // lock for NodeList and IConfMix::clientNum
    sem_t semCount; // for encoder thread

    List<sp<IMediaNode> >mNodeList;
    mixMeta mMetas[MAX_VIDEO_ITEMS];

    uint32_t mEncoderNum;
    uint32_t mCameraNum;

private:
    VideoConfMix();
    VideoConfMix(const VideoConfMix&);
    void operator=(const VideoConfMix&);
};

#endif

