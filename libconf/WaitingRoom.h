#ifndef WAITING_ROOM_H
#define WAITING_ROOM_H

#include <utils/threads.h>
#include <utils/Vector.h>
#include <list>
#include <algorithm>

#include "IMultiParty.h"

using namespace android;
using std::list;

class WaitingRoom : public IConfMix
{
public:
    static WaitingRoom *create();

    virtual ~WaitingRoom();

    virtual status_t start();
    virtual status_t reset();

    virtual void dumpInfo(void *stream);

    // Thread interface
    virtual bool threadLoop();

private:
    Mutex mLockQ;

    int mSelectTimer;

    sp<IMediaNode> mMedia;
    status_t initMixer();

    // counter of voip lines
    int mAudioNum;
    int mVideoNum;

    list<IMediaNode* >mVideoList;
    list<IMediaNode* >mAudioList;

    virtual status_t doInsertNode(IMediaNode *node);
    virtual status_t doRemoveNode(IMediaNode *node);

    WaitingRoom();
    WaitingRoom(const WaitingRoom&) {};
    void operator=(const WaitingRoom&) {};
};

#endif

