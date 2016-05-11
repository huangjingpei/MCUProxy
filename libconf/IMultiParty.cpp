#include "IMultiParty.h"
#include "CompoundMix.h"
#include "WaitingRoom.h"
#include "AudioConfMix.h"
#include "VideoConfMix.h"

#include "sh_print.h"
#include "ioStream.h"
#define LOG_TAG "IMultiParty"
#include "utility.h"

IConfMix* IConfMix::create(ConfType type)
{
    IConfMix* self = NULL;
    status_t err = UNKNOWN_ERROR;

    do
    {
        if (CONF_FULL_MEDIA == type) {
            self = CompoundMix::create();
        } else if (CONF_IDLE_MEDIA == type) {
            self = WaitingRoom::create();
        } else if (CONF_VIDEO_ONLY == type) {
            self = VideoConfMix::create();
        } else if (CONF_AUDIO_ONLY == type) {
            self = AudioConfMix::create();
        }
        BREAK_IF(self == NULL);
        err = NO_ERROR;
    }while(0);

    if (err != NO_ERROR && self != NULL) {
        delete self;
        self = NULL;
    }

    return self;
}

