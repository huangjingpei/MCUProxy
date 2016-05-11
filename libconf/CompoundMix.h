#ifndef COMPOUND_MIXER_H
#define COMPOUND_MIXER_H

#include "IMultiParty.h"

class CompoundMix : public IConfMix
{
public:
    static CompoundMix *create();
    virtual ~CompoundMix() { mAudioMix = mVideoMix = NULL; };

    virtual status_t start();
    virtual status_t reset();

    virtual void dumpInfo(void *stream);

    // Thread interface
    // TODO: for audio/video sync
    virtual bool threadLoop() { return false; };

private:
    ///Mutex mLock;  // lock for reentrancy

    sp<IConfMix> mAudioMix;
    sp<IConfMix> mVideoMix;

    status_t doUpdateClientNum();
    virtual status_t doInsertNode(IMediaNode *node);
    virtual status_t doRemoveNode(IMediaNode *node);

    CompoundMix() {};
    CompoundMix(const CompoundMix&) {};
    void operator=(const CompoundMix&) {};
};

#endif

