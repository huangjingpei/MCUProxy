#include "ToneGene.h"

#define LOG_TAG "ToneGene"
#include "utility.h"


/// singleton
Mutex ToneGene::cLock;
sp<ToneGene> ToneGene::instance;

// TODO: would it be possible that we remove read/write index?
// BUG:
// 1. read write index is not thread-safe
// 2. read write index increse step, samplerate should be sync with real AudioFrame
DtmfGene::DtmfGene() :
    dtVal(-1)
{
    for(int i=0; i<10; i++)
        dtu[i].reinit = 1;
}

status_t DtmfGene::SetDtmf(char keyVal)
{
    dtVal = keyVal;
    return NO_ERROR;
}

//status_t DtmfGene::updateFrame(AudioFrame &_frame, bool inAudio)
status_t DtmfGene::updateFrame(int16_t *frame, uint32_t nSamples, uint16_t value, int16_t inAudio)
{
    status_t err = UNKNOWN_ERROR;

    if (frame == NULL || nSamples < EVENT_SAMPLES || (value<0 || value >32)) {
        return BAD_VALUE;
    }
    gen(inAudio, value);

    memcpy(&frame[0], dtmfBuf, sizeof(short)*EVENT_SAMPLES);


    return NO_ERROR;
}

int DtmfGene::gen(int16_t inAudio, uint16_t value)
{
    short genLength;

    genLength = GenDtmf(&dtu[inAudio], dtVal, value, dtmfBuf,
                        CONST_SAMPLING_RATE, EVENT_SAMPLES);


    return genLength;
}

ToneGene::ToneGene() :
    toneInTx(false),
    toneGen_INF(false),
    cadNow(NULL),
    freq1(0),
    freq2(0)
{
}


status_t ToneGene::setToneScript(char *tones)
{
    status_t err = NO_ERROR;

    // do parse
    char *cursor = tones;
    char *loop = NULL;

    Mutex::Autolock _l(mLock);

    if (strcmp("0000", tones) == 0){
        while(!mToneList.empty()){
           mToneList.erase(mToneList.begin());
        }
        if (cadNow != NULL){
            cadNow->timeOn = 50;
            cadNow->timeOff = 50;
        }
        toneGen_INF = false;
        return err;
    }

    do
    {
        loop = strsep(&cursor, ";");
        DOING_IF(loop != NULL);

        LOGV("loop token: %s", loop);

        char buffer[64];
        char loopTone[256] ;
        int loopTime = 0;


        memset(buffer, 0 , 64);
        memset(loopTone, 0 , 256);
        sscanf(loop, "%[^(](%[^)])", buffer, loopTone);
        sscanf(buffer, "%d", &loopTime);
        LOGV("loop time %d seconds, for tone %s\n", loopTime, buffer);

        if (loopTime == 0) {
            toneGen_INF = true;
        }

        Cadence _cadences[6]; // MAX

        uint32_t _index = 0;
        uint64_t _divider = 0;
        char *note = loopTone;
        char *caden = NULL;

        do {
            uint32_t timeOn, timeOff;
            uint32_t  freq1, freq2;

            caden = strsep(&note, ",");
            DOING_IF(caden != NULL);

            timeOn = timeOff = freq1 = freq2 = 0;
            sscanf(caden, "%lld/%lld/%u+%u",
                    &timeOn, &timeOff, &freq1, &freq2);

            // TODO: check if it is valid
            _cadences[_index].timeOn = timeOn;
            _cadences[_index].timeOff = timeOff;
            _cadences[_index].freq1 = freq1;
            _cadences[_index].freq2 = freq2;

            _divider += (timeOn + timeOff);
        } while (++_index < 6 && caden != NULL); // MAX

        if (_divider == 0) {
            continue;  // invalid, parse next
        }

        list<Cadence> _list;
     //   loopTime *= 1000; // turn into ms

#if 1 // as cycle
        uint32_t _cycle = loopTime / _divider;
        int i;

        if (loopTime == 0){
            _cycle = 1;
        }

        LOGV("register candence %u x %u", _cycle, _index);
        while (_cycle-- != 0)
        {
            for (i=0; i<_index; ++i)
            {
                _list.push_back(_cadences[i]);
                //setCadence(_cadences[i]);
            }
        }
#else // as ration
#endif
        {
            mToneList.clear(); // set by replace
            mToneList.splice(mToneList.end(), _list);
        }
    }while(loop != NULL);

    return err;
}

/***
status_t ToneGene::setCadence(Cadence cad)
{
    Mutex::Autolock _l(mLock);
    mToneList.push_back(cad);

    return NO_ERROR;
}

status_t ToneGene::setCadence(list<Cadence> cadList)
{
    Mutex::Autolock _l(mLock);
    mToneList.clear(); // set by replace
    mToneList.splice(mToneList.end(), cadList);
    return NO_ERROR;
}
**/

bool ToneGene::empty()
{
    if (cadNow == NULL) {
        Mutex::Autolock _l(mLock);
        return (mToneList.empty());
    } else {
        return false;
    }
}

// NOTICE: this method suppose to be
//         refreshed (refresh flag as 'true') for every 10ms
///status_t ToneGene::updateFrame(AudioFrame &_frame, bool refresh)
status_t ToneGene::updateFrame(int16_t *frame, uint32_t nSamples, uint16_t value , bool refresh)
{
    if (frame == NULL || nSamples *sizeof(short)< sizeof(toneBuf) || ( value < 0 ||  value > 36)) {
        return BAD_VALUE;
    }
    Mutex::Autolock _l(mLock);
    if (cadNow == NULL) {

        if (mToneList.empty()) {
            return NO_ERROR;
        } else {
            cadNow = new Cadence();
            if (cadNow == NULL) {
                return NO_MEMORY;
            } else {
                *cadNow = *mToneList.begin();
                mToneList.erase(mToneList.begin());

                if (toneGen_INF){
                    mToneList.push_back(*cadNow);
                }
                if ((freq1 != cadNow->freq1) || (freq2 != cadNow->freq2)){
                    freq1 = cadNow->freq1;
                    freq2 = cadNow->freq2;
                    dtmfdec.reinit = 1;
                }
                get_tone_ceff(14, FS_HZ, freq1, &yInit[0], &aTbl[0]);
                get_tone_ceff(14, FS_HZ, freq2, &yInit[1], &aTbl[1]);
            }
        }
    }

    if (cadNow == NULL) { // just for safe
        return UNKNOWN_ERROR;
    }

    if (refresh) { // we refresh tone for local speaker
        if (cadNow->timeOn > 0) {
            GenTone(&dtmfdec,yInit, aTbl, value , toneBuf, FS_HZ, 160);
            ///LOGV( "Tone value : %d \n", value);
            // TODO: generate tone and update
            cadNow->timeOn -= 10; // 10 ms
        } else if (cadNow->timeOff > 0) {
            memset(toneBuf, 0, sizeof(toneBuf));
            // update with zero?
            cadNow->timeOff -= 10;
        }

        //LOGV("cadence generating info %d/%d.", cadNow->timeOn, cadNow->timeOff);
        if (cadNow->timeOn <= 0 && cadNow->timeOff <= 0) {
            //LOGV("cadence generating done.");
            delete cadNow;
            cadNow = NULL;
        }
    } else if (!toneInTx) { // if it is copy case, which means for Tx
        // toneInTx is not set, just return
        return NO_ERROR;
    }

    //memcpy(&_frame.data_[0], toneBuf, sizeof(toneBuf));
    memcpy(&frame[0], toneBuf, sizeof(toneBuf));

    return NO_ERROR;
}

