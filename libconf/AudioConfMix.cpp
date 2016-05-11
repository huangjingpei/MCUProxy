#include "AudioConfMix.h"

#include "sh_print.h"
#include "ioStream.h"

#define LOG_TAG "AudioMix"
#include "utility.h"

unsigned int AudioConfMix::mSourceNum = MAX_MIX_OBJECT;
unsigned int AudioConfMix::mSampleLen = EVENT_SAMPLES;

extern "C"
{
void fastmix_pre_amp(fastmix_io_t *pInput,
                        int nNum, int nLen);

void fastmix_sat_amp(int *pInput, fastmix_io_t *pOutput, int nLen);

void fastmix_add(fastmix_io_t *pInput, int *pOutput,
                 int nNum, int nLen,
                 int *ditherState);

void fastmix_sub_amp(int *buffer,
                     fastmix_io_t *pInput,
                     fastmix_io_t *pOutput,
                     int nNum, int nLen);
};

AudioConfMix* AudioConfMix::create()
{
    F_LOG;

    AudioConfMix* self = NULL;
    status_t err = UNKNOWN_ERROR;

    do
    {
        self = new AudioConfMix();
        BREAK_IF(self == NULL);

        err = self->initMixer();
    }while(0);

    if (err != NO_ERROR && self != NULL) {
        delete self;
        self = NULL;
    }

    return self;
}

AudioConfMix::AudioConfMix() :
        mMixNum(0),
        mstate(false)
{
    mixMeta *pMeta;
    for (int i=0; i<MAX_MIX_OBJECT; ++i)
    {
        pMeta = &mMetas[i];
        memset(pMeta->txData, 0, sizeof(pMeta->txData));
        memset(pMeta->rxData, 0, sizeof(pMeta->rxData));

        pMeta->txBuffer = MetaBuffer::create(sizeof(pMeta->txData),
                                              pMeta->txData);

        pMeta->rxBuffer = MetaBuffer::create(sizeof(pMeta->rxData),
                                              pMeta->rxData);

        pMeta->txBuffer->setInt32("samples",
                                               (int32_t)mSampleLen);

        AudioIn[i].pAudioData = (short *)pMeta->rxData;
        AudioOut[i].pAudioData = (short *)pMeta->txData;
    }
}

AudioConfMix::~AudioConfMix()
{
    while (!mNodeList.empty()) {
        (*mNodeList.begin())->decStrong(this);
        mNodeList.erase(mNodeList.begin());
    }

    mixMeta *pMeta;
    for (int i=0; i<MAX_MIX_OBJECT; ++i)
    {
        pMeta = &mMetas[i];
        pMeta->txBuffer = NULL;
        pMeta->rxBuffer = NULL;
    }
    delete [] buffer;
}

status_t AudioConfMix::initMixer()
{
    Mutex::Autolock _l(mMixLock);
    buffer = new int[mSampleLen];

    return (buffer != NULL)? NO_ERROR : NO_MEMORY;
}

status_t AudioConfMix::start()
{
    status_t err = NO_ERROR;

    do
    {
        Mutex::Autolock _l(mMixLock);

        list<sp<IMediaNode> >::iterator it;
#if 0   // TODO: start by push or pull?
        for (it = mNodeList.begin(); it != mNodeList.end(); ++it)
        {
            err = (*it)->start();
            BREAK_IF(err != NO_ERROR);
        }
        BREAK_IF(it != mNodeList.end());
#endif
        if (!mstate){
            err = run();
        }
    }while(0);

    return err;
}

status_t AudioConfMix::reset()
{
    //F_LOG;
    status_t err = UNKNOWN_ERROR;

    do
    {

        requestExit();
        mAvailable.signal();
#if 0   // so as to keep APM inside
        Mutex::Autolock _l(mMixLock);
        // conference reset DO NOT call IMediaNode::reset() of inside
        // nodes, the status of each node has to be maintained by themselves
        while (!mNodeList.empty()) {
            mNodeList.erase(mNodeList.begin());
        }

        IConfMix::mClientNum = 0;
#endif
        err = NO_ERROR;
    }while(0);

    return err;
}

status_t AudioConfMix::setParameters(const KeyedVector<String8, String8>& \
                                             params, uint32_t mid)
{
    //F_LOG;
    return INVALID_OPERATION;
}

status_t AudioConfMix::mix(fastmix_io_t *pInput,
                        fastmix_io_t *pFull, fastmix_io_t *pInter)
{
    if (pInput == NULL || buffer == NULL) {
        return -1;
    }

    fastmix_pre_amp(pInput, mSourceNum, mSampleLen);

    fastmix_add(pInput, buffer, mSourceNum, mSampleLen, NULL);

    if (pFull != NULL) {
        fastmix_sat_amp(buffer, pFull, mSampleLen);
    }
    if (pInter != NULL) {
        fastmix_sub_amp(buffer, pInput, pInter, mSourceNum, mSampleLen);
    }

    return NO_ERROR;
}

bool AudioConfMix::threadLoop()
{
    mstate = true;
    int logCount = 0;
    while (!exitPending())
    {
        int64_t _ts;
        int ret, i;
        int _size;

        for (i=0; i<MAX_MIX_OBJECT; ++i)
        {
            AudioIn[i].amp = AudioOut[i].amp = MIX_DISABLE;
        }

        {
            Mutex::Autolock _l(mMixLock);

            list<IMediaNode* >::iterator it;

            mAvailable.waitRelative(mMixLock, 5*1000000);
            if (exitPending()) {
                break;
            }

            if(IConfMix::mClientNum) {
                for (it = mNodeList.begin(), i=0; it != mNodeList.end(); ++it, ++i)
                {
                    ret = (*it)->pullMedia(mMetas[i].rxBuffer);
                    if (ret != NO_ERROR) {
    #if 0
                        logCount++;
                        if (logCount > 10){
                            LOGE("member %d pull error, skip", i);
                            logCount = 0;
                        }
    #endif
                        break;
                    }

                    uint32_t _vol = mMetas[i].rxBuffer->quickData();
                    if ((0 == i) || (_vol == 1)) {
                        AudioIn[i].amp = MIX_KEEP_0dB;
                        AudioOut[i].amp = MIX_KEEP_0dB;
                    } else {
                        ///LOGV("mute voip %d", (i-1));
                        AudioIn[i].amp = MIX_DISABLE;
                        AudioOut[i].amp = MIX_KEEP_0dB;
                    }
                }
                if (ret != NO_ERROR)
                    continue;

                _size = mMetas[0].rxBuffer->size();
                mMetas[0].rxBuffer->findInt64("ntp-time", &_ts);

                mix(AudioIn, NULL, AudioOut);

                for (it = mNodeList.begin(), i=0; it != mNodeList.end(); ++it, ++i)
                {
                    mMetas[i].txBuffer->setRange(0, _size);
                    mMetas[i].txBuffer->setInt64("ntp-time",
                                                           (int64_t)_ts);

                    ret = (*it)->pushMedia(mMetas[i].txBuffer);
                    if (ret != NO_ERROR) {
                        LOGE("member %d push error, skip", i);
                        continue;
                    }
                }


                logCount = 0;
            }
            else
            {
               //assume 0:ACM  1:VSCM (virtual sound card )
               it = mNodeList.begin();
               (*it++)->pullMedia(mMetas[0].rxBuffer);
               (*it)->pullMedia(mMetas[1].rxBuffer);

               it = mNodeList.begin();
               (*it++)->pushMedia(mMetas[1].rxBuffer);
               (*it)->pushMedia(mMetas[0].rxBuffer);
            }
        }
    } // end while

    LOGD("audio mixer thread %d end.", getTid());
    mstate = false;
    return false;
}

status_t AudioConfMix::doInsertNode(IMediaNode *node)
{
    //F_LOG;
    status_t err = UNKNOWN_ERROR;
    do
    {
        Mutex::Autolock _l(mMixLock);
        list<IMediaNode* >::iterator it;


        BREAK_IF(mNodeList.size() >= MAX_MIX_OBJECT);

        if (mNodeList.empty()){
            node->incStrong(this);
            mNodeList.push_back(node);
        } else {
            for (it=mNodeList.begin(); it!=mNodeList.end(); it++){
                if (node->nodeType() < (*it)->nodeType()){
                    mNodeList.insert(it, node);
                    break;
                }
            }

            if (it == mNodeList.end()){
                mNodeList.push_back(node);
            }
            node->incStrong(this);
        }

        LOGD("%s[%p] join the audio loop", node->nodeName(), node);
        if (node->nodeType() == IMediaNode::NODE_A_IP_CODEC) {
            BREAK_IF(IConfMix::mClientNum >= MAX_VOIP_OBJECT);
            ++IConfMix::mClientNum;
        }

        ++mMixNum;

        err = NO_ERROR;
    }while(0);

    return err;
}

status_t AudioConfMix::doRemoveNode(IMediaNode *node)
{
    //F_LOG;
    status_t err = UNKNOWN_ERROR;
    do
    {
        Mutex::Autolock _l(mMixLock);

        list<IMediaNode* >::iterator it;

        it = std::find(mNodeList.begin(), mNodeList.end(), node);
        if (it != mNodeList.end()) { // found
            (*it)->decStrong(this);
            mNodeList.erase(it);
            LOGD("%s[%p] leaving %p", (*it)->nodeName(), (*it), this);
            if (node->nodeType() == IMediaNode::NODE_A_IP_CODEC) {
                --IConfMix::mClientNum;
            }

            --mMixNum;
        } else {
            LOGV("no target in conference");
        }

        err = NO_ERROR;
    }while(0);

    return err;
}

void AudioConfMix::dumpInfo(void *stream)
{
    char _buffer[1024];
    ioStream _logs, *_plogs;
    _logs.buffer = _buffer;
    _logs.size = sizeof(_buffer);

    ioStreamRewind(&_logs);
    _plogs = (stream == NULL) ? &_logs : (ioStream *)stream;

    Mutex::Autolock _l(mMixLock);

    list<IMediaNode* >::iterator it;

    ioStreamPrint(_plogs, " \\ audio/%d: ", clientNum());
    if (mNodeList.size() != 0) {
        for (it = mNodeList.begin(); it != mNodeList.end(); ++it)
        {
            if ((*it)->isGood()) {
                ioStreamPrint(_plogs, "%s[%p/%d] -> ", (*it)->nodeName(),
                              (*it), (*it)->getStrongCount());
            } else {
                ioStreamPrint(_plogs, "%s|%p/%d| -> ", (*it)->nodeName(),
                              (*it), (*it)->getStrongCount());
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

