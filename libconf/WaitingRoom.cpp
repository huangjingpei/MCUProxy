#include "defines.h"
#include "WaitingRoom.h"
#include "MOHPlayer.h"

#include "sh_print.h"
#include "ioStream.h"

#define LOG_TAG "WaitingRoom"
#include "utility.h"


static uint32_t GetTimeInMS()
{
    struct timeval tv;
    struct timezone tz;
    uint32_t val;

    gettimeofday(&tv, &tz);
    val = (uint32_t)(tv.tv_sec*1000 + tv.tv_usec/1000);
    return val;
}

WaitingRoom* WaitingRoom::create()
{
    //F_LOG;

    WaitingRoom* self = NULL;
    status_t err = UNKNOWN_ERROR;

    do
    {
        self = new WaitingRoom();
        BREAK_IF(self == NULL);

        err = self->initMixer();
    }while(0);

    if (err != NO_ERROR && self != NULL) {
        delete self;
        self = NULL;
    }

    return self;
}

WaitingRoom::WaitingRoom()
{   // TODO: enable pull video data from decoder and remove this timer
    mSelectTimer = EVENTTIME_MS;
    mAudioNum = mVideoNum = 0;
}

WaitingRoom::~WaitingRoom()
{
    reset();

    mMedia = NULL;
    while (!mAudioList.empty()) {
        (*mAudioList.begin())->decStrong(this);
        mAudioList.erase(mAudioList.begin());
    }

    while (!mVideoList.empty()) {
        (*mVideoList.begin())->decStrong(this);
        mVideoList.erase(mVideoList.begin());
    }

    mAudioNum = mVideoNum = 0;
    IConfMix::mClientNum = 0;
}

status_t WaitingRoom::initMixer()
{
    status_t err = UNKNOWN_ERROR;

    do
    {
        Mutex::Autolock _l(mLockQ);

        mMedia = MOHPlayer::create(mSelectTimer, "/tmp/moh.pcm");
        BREAK_IF(mMedia == NULL);
#if 0
        err = insertNode(_node.get());
        BREAK_IF(err != NO_ERROR);
#else   // MOH node is not used as migrant node, so add it privatly
        mMedia->incStrong(this);
        mAudioList.push_front(mMedia.get());
#endif
        err = NO_ERROR;
    }while(0);

    return err;
}

status_t WaitingRoom::start()
{
    //F_LOG;
    status_t err = UNKNOWN_ERROR;

    do
    {
        Mutex::Autolock _l(mLockQ);

        err = mMedia->start();
        BREAK_IF(err != NO_ERROR);

        err = run();
    }while(0);

    return err;
}

status_t WaitingRoom::reset()
{
    //F_LOG;
    status_t err = UNKNOWN_ERROR;

    do
    {
        Mutex::Autolock _l(mLockQ);

        err = mMedia->reset();
        BREAK_IF(err != NO_ERROR);

        requestExit();

        err = NO_ERROR;
    }while(0);

    return err;
}


status_t WaitingRoom::doInsertNode(IMediaNode *node)
{
    //F_LOG;
    status_t err = BAD_VALUE;

    do
    {
        BREAK_IF(node == NULL);
        err = NO_ERROR;

        IMediaNode::NodeType _type = node->nodeType();
        // only accept the nodes of voip client type
        if (_type == IMediaNode::NODE_A_IP_CODEC) {
            node->incStrong(this);
            mAudioList.push_back(node);
            ++mAudioNum;
        } else if (_type == IMediaNode::NODE_V_IP_RENDER) {
            node->incStrong(this);
            mVideoList.push_back(node);
            ++mVideoNum;
        } else {
            break;
        }

        ++IConfMix::mClientNum;
    }while(0);

    return err;
}

status_t WaitingRoom::doRemoveNode(IMediaNode *node)
{
    //F_LOG;
    status_t err = BAD_VALUE;

    do
    {
        BREAK_IF(node == NULL);
        Mutex::Autolock _l(mLockQ);
        IMediaNode::NodeType _type = node->nodeType();
        list<IMediaNode* >::iterator it;

        // only accept the nodes of voip client type
        if (_type == IMediaNode::NODE_A_IP_CODEC) {
            it = std::find(mAudioList.begin(), mAudioList.end(), node);
            if (it != mAudioList.end()) { // found
                (*it)->decStrong(this);
                mAudioList.erase(it);
                --mAudioNum;
            }
        } else if (_type == IMediaNode::NODE_V_IP_RENDER) {
            it = std::find(mVideoList.begin(), mVideoList.end(), node);
            if (it != mVideoList.end()) { // found
                (*it)->decStrong(this);
                mVideoList.erase(it);
                --mVideoNum;
            }
        } else {
            break;
        }

        --IConfMix::mClientNum;
        err = NO_ERROR;
    }while(0);

    return err;
}

bool WaitingRoom::threadLoop()
{
    int16_t buffer[EVENT_SAMPLES];
    sp<MetaBuffer> _buffer = MetaBuffer::create(sizeof(buffer), buffer);
    sp<MetaBuffer> _trash = MetaBuffer::create(4096);
    int i, err;

    bool firstRun = true;
    struct timeval tvPrev, tvNow;
    struct timeval waitTime;
    while (!exitPending())
    {
        if (!firstRun) {
            do
            {
                err = select(0, NULL, NULL, NULL, &waitTime);
            }while(err && errno == EINTR);
        }
        firstRun = false;


        gettimeofday(&waitTime, NULL);
        waitTime.tv_usec += EVENTTIME_US;
        {
            Mutex::Autolock _l(mLockQ);

            if (exitPending()) {
                break;
            }

            list<IMediaNode* >::iterator it;

            // TODO: enable pull video data from decoder, the internal moh timer
            //  may not be correct at that time
            for (it = mAudioList.begin(), i=0; it != mAudioList.end(); ++it, ++i)
            {
                if (0 == i) {
                    err = (*it)->pullMedia(_buffer);
                    if (err != NO_ERROR) {
                        LOGE("member %d pull error", i);
                        memset(buffer, 0, sizeof(buffer));
                    }
                } else {
                    err = (*it)->pullMedia(_trash);
                    if (err != NO_ERROR) {
                        LOGV("member %d pull error", i);
                    }
                    err = (*it)->pushMedia(_buffer);
                    if (err != NO_ERROR) {
                        LOGE("member %d push error", i);
                    }
                }
            }
        }

        gettimeofday(&tvNow, NULL);
        waitTime.tv_sec -= tvNow.tv_sec;
        waitTime.tv_usec -= tvNow.tv_usec;

        if (waitTime.tv_usec < 0) {
            waitTime.tv_usec += 1000000L;
            --waitTime.tv_sec;
        }

        if (waitTime.tv_sec < 0) {
            waitTime = {0, 0};
        }
    } // end while

    return false;
}

void WaitingRoom::dumpInfo(void *stream)
{
    char _buffer[1024];
    ioStream _logs, *_plogs;
    _logs.buffer = _buffer;
    _logs.size = sizeof(_buffer);

    ioStreamRewind(&_logs);
    _plogs = (stream == NULL) ? &_logs : (ioStream *)stream;

    Mutex::Autolock _l(mLockQ);
    list<IMediaNode* >::iterator it;

    ioStreamPrint(_plogs, " \\--------------------------------\n");
    ioStreamPrint(_plogs, " \\ audio/%d: ", mAudioNum);
    for (it = mAudioList.begin(); it != mAudioList.end(); ++it)
    {
        ioStreamPrint(_plogs, "%s[%p/%d] -> ", (*it)->nodeName(),
                      (*it), (*it)->getStrongCount());
    }
    ioStreamSeek(_plogs, -3, SEEK_CUR);
    ioStreamPutc('\n', _plogs);

    ioStreamPrint(_plogs, " \\ video/%d: ", mVideoNum);
    for (it = mVideoList.begin(); it != mVideoList.end(); ++it)
    {
        ioStreamPrint(_plogs, "%s[%p/%d] -> ", (*it)->nodeName(),
                      (*it), (*it)->getStrongCount());
    }
    ioStreamSeek(_plogs, -3, SEEK_CUR);
    ioStreamPutc('\n', _plogs);

    ioStreamPrint(_plogs, " \\--------------------------------");
    if (stream == NULL) {
        shell_print("%s", _buffer);
    } else {
        ioStreamPutc('\n', _plogs);
    }
}


