#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "VideoConfMix.h"
#include "sh_print.h"
#include "ioStream.h"

#define LOG_TAG "VideoMix"
#include "utility.h"

VideoConfMix::VideoConfMix()
{
    mCameraNum = 0;
    mEncoderNum = 0;

    sem_init(&semCount, 0, 0);
}

VideoConfMix::~VideoConfMix()
{
}

VideoConfMix* VideoConfMix::create()
{
    F_LOG;

    VideoConfMix* self = NULL;
    status_t err = UNKNOWN_ERROR;

    do
    {
        self = new VideoConfMix();
        BREAK_IF(self == NULL);

        err = self->init();
    }while(0);

    if (err != NO_ERROR && self != NULL) {
        delete self;
        self = NULL;
    }

    return self;
}

status_t VideoConfMix::init()
{
    status_t err = NO_MEMORY;

    do
    {
        //Mutex::Autolock _l(mMixLock);
        int i;

        err = NO_ERROR;
    }while(0);

    return NO_ERROR;
}



status_t VideoConfMix::start()
{
    F_LOG;
    status_t err = NO_ERROR;
    MARK_LINE;
    do
    {
        DOING_IF(Thread::getTid() == -1);

        Mutex::Autolock _l(mLockQ);

        List<sp<IMediaNode> >::iterator it;
        for (it = mNodeList.begin(); it != mNodeList.end(); ++it)
        {   MARK_LINE;
            err = (*it)->start();
            BREAK_IF(err != NO_ERROR);
        }
        BREAK_IF(it != mNodeList.end());

        err = run();
        //BREAK_IF(err != NO_ERROR);

        //mRunning = true;
    }while(0);

    return err;
}

status_t VideoConfMix::reset()
{
    F_LOG;
    status_t err = NO_ERROR;

    do
    {
        //DOING_IF(mRunning);

        //LOGV("stop in running case");
        sem_post(&semCount);

        err = requestExitAndWait();
        BREAK_IF(err != NO_ERROR);
        Mutex::Autolock _l(mLockQ);
#if 0   // so as to keep VCM inside
        // conference reset DO NOT call IMediaNode::reset() of inside
        // nodes, the status of each node has to be maintained by themselves
        while (!mNodeList.empty()) {
            mNodeList.erase(mNodeList.begin());
        }
#endif
        IConfMix::mClientNum = 0;
    }while(0);

    return err;
}

status_t VideoConfMix::setParameters(KeyedVector<String8, String8>& params)
{
    return INVALID_OPERATION;
}

bool VideoConfMix::threadLoop()
{
    //F_LOG;
    int err, i;

    while (!exitPending())
    {
        do
        {
            err = sem_trywait(&semCount);
        }while(err && errno == EINTR);

        if (!err && exitPending()) {
            break;
        }

        List<sp<IMediaNode> >::iterator it;
        int offsetDec, offsetEnc;

        {
            Mutex::Autolock _l(mLockQ);
            for (it = mNodeList.begin(), i=0; it != mNodeList.end(); ++it, ++i)
            {
                mMetas[i].rxBuffer = NULL;
                err = (*it)->pullMedia(mMetas[i].rxBuffer);
                /*
                if (err != NO_ERROR) {
                    LOGV("member %d pull error", i);
                }*/
            }

            offsetDec = mCameraNum;
            offsetEnc = mCameraNum + IConfMix::mClientNum;
        }

        // do mix, assume video camera at head
        // TODO: mix 3-way picture(rxBuffer 0~2),
        // txBuffer 0 push to encoder?
#if 0
        uint32_t _overlapSize = mOverlapWidth * mOverlapHeight;
        _overlapSize += (_overlapSize >> 1);

        FastMemory::Instance()->allocate(m3WayBuffer,
                                         _overlapSize);
        if (m3WayBuffer != NULL) {
            /// ASSERT(mOverlapBuffer != NULL);
            mPhyAddr = FastMemory::Instance()->toPhysical(m3WayBuffer->data());
            mPhyAddrUV = mPhyAddr + (mOverlapWidth * mOverlapHeight);

            overlapPicture(mMetas[0].rxBuffer, 240, 30, 320, 240);
            overlapPicture(mMetas[1].rxBuffer, 50, 300, 320, 240);
            overlapPicture(mMetas[2].rxBuffer, 430, 300, 320, 240);
        }
#endif

        for (i=0; i<IConfMix::mClientNum; ++i)
        {
            mMetas[offsetDec+i].txBuffer = mMetas[offsetEnc].rxBuffer;
        }

        if (offsetDec != 0) { // means 'mCameraNum!=0', offsetDec is thread-safe
            mMetas[offsetEnc].txBuffer = mMetas[0].rxBuffer;
        }

        {
            Mutex::Autolock _l(mLockQ);

            // detect if there is any change
            if ((offsetDec != mCameraNum) || \
                (offsetEnc != (mCameraNum + IConfMix::mClientNum))) {
                continue; // illegal, do it again
            }

            for (it = mNodeList.begin(), i=0; it != mNodeList.end(); ++it, ++i)
            {
                err = (*it)->pushMedia(mMetas[i].txBuffer);
                /*if (err != NO_ERROR) {
                    LOGV("member %d push error", i);
                    break;
                }*/
            }
        }
    }

    LOGD("video mixer thread %d end.", getTid());
    //mRunning = false;
    return false;
///#endif
///    return true;
}

status_t VideoConfMix::doInsertNode(IMediaNode *node)
{
    F_LOG;
    status_t err = UNKNOWN_ERROR;
    do
    {
        IMediaNode::NodeType _type;

        BREAK_IF(node == NULL);
        _type = node->nodeType();

        Mutex::Autolock _l(mLockQ);

        List<sp<IMediaNode> >::iterator it;
        for (it=mNodeList.begin(); it!=mNodeList.end(); it++)
        {
            if (_type < (*it)->nodeType()) {
                break;
            }
        }

        if (IMediaNode::NODE_V_IP_RENDER == _type) {
            BREAK_IF(IConfMix::mClientNum >= MAX_VIDEO_CLIENT);
            ++IConfMix::mClientNum;
        } else if (IMediaNode::NODE_V_CAMERA_SET == _type) {
            BREAK_IF(mCameraNum >= MAX_VIDEO_CAMERA);
            ++mCameraNum;
        } else if (IMediaNode::NODE_V_ENCODER) {
            BREAK_IF(mEncoderNum >= MAX_VIDEO_ENCODER);
            ++mEncoderNum;
        } else {
            /// return NO_ERROR for audio nodes
            err = NO_ERROR; //BAD_TYPE;
            break;
        }

        mNodeList.insert(it, node);
        LOGD("%s[%p] join the video loop", node->nodeName(), node);

        err = NO_ERROR;
    }while(0);

    return err;
}

status_t VideoConfMix::doRemoveNode(IMediaNode *node)
{
    F_LOG;
    status_t err = UNKNOWN_ERROR;
    do
    {
        IMediaNode::NodeType _type;

        BREAK_IF(node == NULL);
        _type = node->nodeType();

        Mutex::Autolock _l(mLockQ);

        //mNodeList.remove(node);
        List<sp<IMediaNode> >::iterator it;
        for (it = mNodeList.begin(); it != mNodeList.end(); ++it)
        {
            if ((*it).get() == node) {
                break;
            }
        }

        BREAK_IF(it == mNodeList.end()); // not found
        LOGD("%s[%p] leaving %p", (*it)->nodeName(), (*it).get(), this);
        mNodeList.erase(it);

        if (IMediaNode::NODE_V_IP_RENDER == _type) {
            --IConfMix::mClientNum;
        } else if (IMediaNode::NODE_V_CAMERA_SET == _type) {
            --mCameraNum;
        } else if (IMediaNode::NODE_V_ENCODER) {
            --mEncoderNum;
        }

        err = NO_ERROR;
    }while(0);

    return err;
}

void VideoConfMix::dumpInfo(void *stream)
{
    char _buffer[1024];
    ioStream _logs, *_plogs;
    _logs.buffer = _buffer;
    _logs.size = sizeof(_buffer);

    ioStreamRewind(&_logs);
    _plogs = (stream == NULL) ? &_logs : (ioStream *)stream;

    Mutex::Autolock _l(mLockQ);

    List<sp<IMediaNode> >::iterator it;

    ioStreamPrint(_plogs, " \\ video/%d: ", clientNum());
    if (mNodeList.size() != 0) {
        for (it = mNodeList.begin(); it != mNodeList.end(); ++it)
        {
            if ((*it)->isGood()) {
                ioStreamPrint(_plogs, "%s[%p/%d] -> ", (*it)->nodeName(),
                              (*it).get(), (*it)->getStrongCount());
            } else {
                ioStreamPrint(_plogs, "%s|%p/%d| -> ", (*it)->nodeName(),
                              (*it).get(), (*it)->getStrongCount());
            }
        }
        ioStreamSeek(_plogs, -3, SEEK_CUR);
    }

    if (stream == NULL) {
        ioStreamPutc('\0', _plogs);
        shell_print("%s", _buffer);
    } else {
        ioStreamPutc('\n', _plogs);
    }
}

