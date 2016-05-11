#ifndef MOH_PLAYER_H
#define MOH_PLAYER_H

#include <stdio.h>
#include <stdlib.h>

#include "IMultiParty.h"

using namespace android;

class MOHPlayer : public IMediaNode
{
public:
    static MOHPlayer *create(uint32_t timeInMs,
                             const char *music,
                             const char *record=NULL);

    virtual ~MOHPlayer();

    // IMediaNode interface
    virtual status_t onStart();
    virtual status_t onReset();

    virtual status_t pullMedia(sp<MetaBuffer> &data);
    virtual status_t pushMedia(sp<MetaBuffer> &data);

    virtual NodeType nodeType() {
        return IMediaNode::NODE_A_DRIVER;
    };

    virtual NodeState nodeState() {
        return IMediaNode::NSTATE_READY;
    };

    virtual const char *nodeName() { return "MOH"; };
    virtual void dumpInfo(void *stream);
private:
    FILE *fpMusic;
    FILE *fpWrite;

    bool firstRun;
    int eventTime;
    uint32_t eventSample;
    uint32_t timeStamp;

    void initOpen(const char *music, const char *record);

private:
    MOHPlayer(uint32_t timeInMs);
    MOHPlayer(const MOHPlayer&);
    void operator=(const MOHPlayer&);
};

#endif
