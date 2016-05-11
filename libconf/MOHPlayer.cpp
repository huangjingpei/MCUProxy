#if 0 //3531
#include <media/stagefright/foundation/AMessage.h>
#else
#include "AMessage.h"
#endif

#include "defines.h"

#include "MOHPlayer.h"
#include <unistd.h>

#define LOG_TAG "MOHPlayer"
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

static void SleepMs(int msecs)
{
    struct timespec short_wait;
    struct timespec remainder;
    short_wait.tv_sec = msecs / 1000;
    short_wait.tv_nsec = (msecs % 1000) * 1000 * 1000;
    nanosleep(&short_wait, &remainder);
}

MOHPlayer::MOHPlayer(uint32_t timeInMs)
    : fpMusic(NULL)
    , fpWrite(NULL)
    , firstRun(true)
    , eventTime(timeInMs)
{
    eventSample = timeInMs*FS_HZ/1000;
}

MOHPlayer::~MOHPlayer()
{
    if (fpMusic != NULL) {
        fclose(fpMusic);
        fpMusic == NULL;
    }
    if (fpWrite != NULL) {
        fclose(fpWrite);
        fpWrite = NULL;
    }
}

MOHPlayer* MOHPlayer::create(uint32_t timeInMs,
                             const char *music,
                             const char *record)
{
    //F_LOG;
    MOHPlayer* self = NULL;
    status_t err = UNKNOWN_ERROR;

    do
    {
        BREAK_IF(music == NULL);

        self = new MOHPlayer(timeInMs);
        BREAK_IF(self == NULL);

        self->initOpen(music, record);
    }while(0);
    /*******
    when moh file failed to be opened, we keep the object, but
    the data pulled out turned to be silence
    if (err != NO_ERROR && self != NULL) {
        delete self;
        self = NULL;
    }
    *****/

    return self;
}

void MOHPlayer::initOpen(const char *music,
                             const char *record)
{
    status_t err = UNKNOWN_ERROR;

    do
    {
        if (music != NULL) {
            fpMusic = fopen(music, "rb");
        }

        if (record != NULL) {
            fpWrite = fopen(record, "wb");
        }

        timeStamp = (((uint32_t)rand() & 0x0000FFFF) << 16) |
                     ((uint32_t)rand() & 0x0000FFFF);
        err = NO_ERROR;
    }while(0);

    return;
}

status_t MOHPlayer::onStart()
{
    //F_LOG;
    firstRun = true;
    return NO_ERROR;
}

status_t MOHPlayer::onReset()
{
    firstRun = true;
    return NO_ERROR;
}

status_t MOHPlayer::pullMedia(sp<MetaBuffer> &data)
{
    status_t err = BAD_VALUE;
    int _timer;

    do
    {
        BREAK_IF(data == NULL);
        BREAK_IF(data->capacity() < eventSample*sizeof(int16_t));

        memset(data->data(), 0, sizeof(int16_t)*eventSample);

        if (!firstRun) {
            _timer = (int)(GetTimeInMS() - timeStamp);
            if (_timer < eventTime) {
                SleepMs(eventTime - _timer);
            }
        }

        firstRun = false;

        if (fpMusic != NULL) {
            // assume one channel
            size_t n;
            n = fread(data->data(), sizeof(int16_t), eventSample, fpMusic);
            if (n < eventSample && feof(fpMusic) != 0) {
                LOGV("moh file reach EOF");
                fseek(fpMusic, SEEK_SET, 0);
                // read again
                fread(data->data(), sizeof(int16_t), eventSample, fpMusic);
            }
        }

        timeStamp = GetTimeInMS();
        int64_t _ts = (int64_t)timeStamp * FS_HZ/1000;

        data->setInt64("ntp-time", _ts);
        data->setInt32("samples", (int32_t)eventSample);

        data->setRange(0, eventSample*sizeof(int16_t));

        err = NO_ERROR;
    }while(0);

    return err;
}

status_t MOHPlayer::pushMedia(sp<MetaBuffer> &data)
{   // TODO: limit rec pcm size to keep most recent, for debug purpose
    status_t err = BAD_VALUE;

    do
    {
        BREAK_IF(data == NULL);
        BREAK_IF(data->size() != eventSample*sizeof(int16_t));

        if (fpWrite != NULL) {
            fwrite(data->data(), 1, data->size(), fpWrite);
            fflush(fpWrite);
        }

        err = NO_ERROR;
    }while(0);

    return err;
}

void MOHPlayer::dumpInfo(void *stream)
{
}

