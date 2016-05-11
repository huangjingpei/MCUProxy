#include "CompoundMix.h"

#include "AudioConfMix.h"
#include "VideoConfMix.h"

#include "sh_print.h"
#include "ioStream.h"
#define LOG_TAG "CompoundMix"
#include "utility.h"

///
/// All mutex is disabled for the caller is limited to one thread only
///

CompoundMix* CompoundMix::create()
{
    CompoundMix* self = NULL;
    status_t err = UNKNOWN_ERROR;

    do
    {
        self = new CompoundMix();
        BREAK_IF(self == NULL);

        self->mAudioMix = AudioConfMix::create();
        BREAK_IF(self->mAudioMix == NULL);

        self->mVideoMix = VideoConfMix::create();
        BREAK_IF(self->mVideoMix == NULL);

        err = NO_ERROR;
    }while(0);

    if (err != NO_ERROR && self != NULL) {
        delete self;
        self = NULL;
    }

    return self;
}

status_t CompoundMix::start()
{
    status_t err = UNKNOWN_ERROR;

    do
    {
        /// Mutex::Autolock _l(mLock);
        #if 0
        if (mAudioMix->clientNum() != 0) {
            err = mAudioMix->start();
            BREAK_IF(err != NO_ERROR);
        }
        #else
        err = mAudioMix->start();
        BREAK_IF(err != NO_ERROR);
        #endif

        if (mVideoMix->clientNum() != 0) {
            err = mVideoMix->start();
            BREAK_IF(err != NO_ERROR);
        }

    }while(0);

    if (err != NO_ERROR) {
        reset();
    }
    return err;
}

status_t CompoundMix::reset()
{
    //F_LOG;
    status_t err = UNKNOWN_ERROR;

    do
    {
        /// Mutex::Autolock _l(mLock);
        if (mAudioMix != NULL) {
            err = mAudioMix->reset();
            //BREAK_IF(err != NO_ERROR);
        }

        if (mVideoMix != NULL) {
            err = mVideoMix->reset();
            //BREAK_IF(err != NO_ERROR);
        }

        err = NO_ERROR;
    }while(0);

    return err;
}

status_t CompoundMix::doUpdateClientNum()
{
    status_t err = UNKNOWN_ERROR;

    do
    {
        if (mAudioMix != NULL && mVideoMix != NULL) {
            IConfMix::mClientNum = mAudioMix->clientNum() + mVideoMix->clientNum();
        } else if (mAudioMix != NULL) {
            IConfMix::mClientNum = mAudioMix->clientNum();
        } else if (mVideoMix != NULL) {
            IConfMix::mClientNum = mVideoMix->clientNum();
        } else {
            LOGE("this is not a supposed case");
            break;
        }
        err = NO_ERROR;
    }while(0);

    return err;
}

status_t CompoundMix::doInsertNode(IMediaNode *node)
{
    //F_LOG;
    status_t err = BAD_VALUE;

    do
    {
        BREAK_IF(node == NULL);

        IMediaNode::NodeType _type = node->nodeType();
        int _class = _type & IMediaNode::NODE_CLASS_MASK;
        err = NO_INIT;

        if (_class == IMediaNode::NODE_A_CLASS) {
            BREAK_IF(mAudioMix == NULL);
            err = mAudioMix->insertNode(node);
        } else if (_class == IMediaNode::NODE_V_CLASS) {
            BREAK_IF(mVideoMix == NULL);
            err = mVideoMix->insertNode(node);
        } else {
            LOGE("unknown node type 0x%08x", _type);
            err = UNKNOWN_ERROR;
        }

        BREAK_IF(err != NO_ERROR);
        err = doUpdateClientNum();
    }while(0);

    return err;
}

status_t CompoundMix::doRemoveNode(IMediaNode *node)
{
    //F_LOG;
    status_t err = BAD_VALUE;

    do
    {
        BREAK_IF(node == NULL);

        IMediaNode::NodeType _type = node->nodeType();
        int _class = _type & IMediaNode::NODE_CLASS_MASK;

        err = NO_INIT;
        //LOGV("node class 0x%08X type 0x%08X", _type, _class);
        if (_class == IMediaNode::NODE_A_CLASS) {
            BREAK_IF(mAudioMix == NULL);
            err = mAudioMix->removeNode(node);
        } else if (_class == IMediaNode::NODE_V_CLASS) {
            BREAK_IF(mVideoMix == NULL);
            err = mVideoMix->removeNode(node);
        } else {
            LOGE("unknown node type 0x%08x", _type);
            err = UNKNOWN_ERROR;
        }

        BREAK_IF(err != NO_ERROR);
        err = doUpdateClientNum();
    }while(0);

    return err;
}

void CompoundMix::dumpInfo(void *stream)
{
    char _buffer[1024];
    ioStream _logs, *_plogs;
    _logs.buffer = _buffer;
    _logs.size = sizeof(_buffer);

    ioStreamRewind(&_logs);
    _plogs = (stream == NULL) ? &_logs : (ioStream *)stream;

    ioStreamPrint(_plogs, " \\--------------------------------\n");
    if (mAudioMix != NULL && mVideoMix != NULL) {
        ioStreamPrint(_plogs, " \\ Compound audio/%d video/%d threads\n",
                   mAudioMix->getTid(), mVideoMix->getTid());
        mAudioMix->dumpInfo(_plogs);
        mVideoMix->dumpInfo(_plogs);
    } else if (mAudioMix != NULL) {
        ioStreamPrint(_plogs, " \\ Compound single audio thread %d\n",
                      mAudioMix->getTid());
        mAudioMix->dumpInfo(_plogs);
    } else if (mVideoMix != NULL) {
        ioStreamPrint(_plogs, " \\ Compound single video thread %d\n",
                      mVideoMix->getTid());
        mVideoMix->dumpInfo(_plogs);
    }
    ioStreamPrint(_plogs, " \\--------------------------------");
    if (stream == NULL) {
        shell_print("%s", _buffer);
    } else {
        ioStreamPutc('\n', _plogs);
    }
}

