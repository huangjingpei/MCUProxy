
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>


#include "wallClock.h"
#include "RTPTransport.h"

#define LOG_TAG "RTPTransport"
#include "utility.h"

#define RTCP_PERIOD  (5000)

static uint16_t u16at(const uint8_t *data) {
    return data[0] << 8 | data[1];
}

static uint32_t u32at(const uint8_t *data) {
    return u16at(data) << 16 | u16at(&data[2]);
}

static uint64_t u64at(const uint8_t *data) {
    return (uint64_t)(u32at(data)) << 32 | u32at(&data[4]);
}

const int RTPTransport::MaxCapacity = 1664;

RTPTransport::RTPTransport(bool rtcp)
    : mHandler(this), mTimer(this)
{
    ///timestamp_ = (((uint32_t)rand() & 0x0000FFFF) << 16) |
    ///    ((uint32_t)rand() & 0x0000FFFF);
    useRTCP = rtcp;
    symmRTP = false; //true;
    firstRTP = true;
    mObserver = NULL;
    mEventId = -1;

    memset(&mInfo, 0, sizeof(mInfo));
}

RTPTransport::~RTPTransport()
{
    useRTCP = false;
    symmRTP = false; //true;
    firstRTP = true;
    mObserver = NULL;

    if (mEventId != -1) {
        SocketPoller::Instance()->DiscardEvent(mEventId);
    }
}

RTPTransport* RTPTransport::create(IRTPNotify *observer, bool rtcp)
{
    RTPTransport* self = NULL;
    status_t err = UNKNOWN_ERROR;

    do
    {
        self = new RTPTransport(rtcp);
        BREAK_IF(self == NULL);

        err = self->init();
        BREAK_IF(err != NO_ERROR);

        err = self->setObserver(observer);
        BREAK_IF(err != NO_ERROR);

        err = NO_ERROR;
    }while(0);

    if (err != NO_ERROR && self != NULL) {
        delete self;
        self = NULL;
    }

    return self;
}

status_t RTPTransport::init()
{
    status_t err = NO_MEMORY;
    do
    {
        if (useRTCP) {
            mRTCP = new udpTransport();
            BREAK_IF(mRTCP == NULL);

            err = mRTCP->init();
            BREAK_IF(err != NO_ERROR);

            mEventId = SocketPoller::Instance()->AddTimedCallback(RTCP_PERIOD,
                                                                     &mTimer);
            BREAK_IF(mEventId == -1);
        }

        err = udpTransport::init();
    }while(0);

    if (err != NO_ERROR && mRTCP != NULL) {
        mRTCP = NULL;  // means 'delete' op
    }

    return err;
}

status_t RTPTransport::setHandler(INetHandler *callback)
{
    status_t err = UNKNOWN_ERROR;
    do
    {
        err = Transport::setHandler(callback);
        BREAK_IF(err != NO_ERROR);

        if (useRTCP) {
            if (callback) {
                err = mRTCP->setHandler(static_cast<INetHandler *>(&mHandler));
            } else {
                err = mRTCP->setHandler(NULL);
            }
        }
    }while(0);

    return err;
}

status_t RTPTransport::setObserver(IRTPNotify *observer)
{
    mObserver = observer;
    return NO_ERROR;
}

status_t RTPTransport::setBindPort(uint16_t port)
{
    //F_LOG;
    status_t err = BAD_VALUE;

    do
    {
        BREAK_IF((port & 0xFFFF0000) != 0);
        if (useRTCP) {
            BREAK_IF((port & 0x00000001) != 0);
        }

        sockaddr_in _addr;
        _addr.sin_family = AF_INET;
        _addr.sin_port = htons(port);
        _addr.sin_addr.s_addr = htonl(INADDR_ANY);

        err = udpTransport::bindAddress(_addr);
        BREAK_IF(err != NO_ERROR);
        if (useRTCP) {
            _addr.sin_port = htons(port +1);
            err = mRTCP->bindAddress(_addr);
            BREAK_IF(err != NO_ERROR);
            mRTCP->getHostAddr(mHostAddr);
            LOGV("RTP/RTCP Port: %d/%d", port, port+1);
        } else {
            LOGV("RTP Port: %d", port);
        }
    }while(0);

    return err;
}

status_t RTPTransport::setSourceID(uint32_t ssrc)
{
    //Mutex::Autolock _l(mLock);
    mSourceID = ssrc;
    return NO_ERROR;
}

// TODO: default ptype according to media type
status_t RTPTransport::setPayloadType(int pt)
{
    //Mutex::Autolock _l(mLock);
    mPType = pt;
    return NO_ERROR;
}

status_t RTPTransport::setTarget(sockaddr_in &_addr, bool symmetric)
{
    mRTPAddr = _addr;
    if (useRTCP) {
        mRTCPAddr = _addr;
        mRTCPAddr.sin_port = htons(ntohs(mRTCPAddr.sin_port) +1);

    }
    symmRTP = symmetric;
    return NO_ERROR;
}


status_t RTPTransport::deliver(const sp<MetaBuffer> &buffer, int64_t timeUs,
                               bool video, bool marker)
{
#if 1
    if (mSocket == -1) return BAD_VALUE;
    buffer->setInt32("fd", mSocket);
    return SocketSender::Instance()->deliver(buffer, timeUs, video, marker);
#else
    return doSend(buffer, timeUs, video, marker);
#endif
}

status_t RTPTransport::doSend(const sp<MetaBuffer> &buffer, int64_t timeUs,
                               bool video, bool marker)
{
    uint32_t rtpTime;
    if (video) {
        //rtpTime = mRTPTimeBase + (timeUs * 9 / 100ll);
        rtpTime = mRTPTimeBase + (timeUs * 90); // temp change us to ms, which actually is
    } else { // current interface for audio
        rtpTime = timeUs;
    }

    if (12 <= buffer->capacity()) {
        // The data fits into a single packet
        uint8_t *data = buffer->data();
        data[0] = 0x80;
        data[1] = (marker ? 0x80 : 0x00) | mPType;  // M-bit
        data[2] = (mSeqNo >> 8) & 0xff;
        data[3] = mSeqNo & 0xff;
        data[4] = rtpTime >> 24;
        data[5] = (rtpTime >> 16) & 0xff;
        data[6] = (rtpTime >> 8) & 0xff;
        data[7] = rtpTime & 0xff;
        data[8] = mSourceID >> 24;
        data[9] = (mSourceID >> 16) & 0xff;
        data[10] = (mSourceID >> 8) & 0xff;
        data[11] = mSourceID & 0xff;

        // no change for others

        udpTransport::deliver(buffer, mRTPAddr);

        ++mSeqNo;
        ++mInfo.nRTPSent;
        mInfo.nRTPOctetsSent += buffer->size() - 12;
    } else {
        LOGE("no room for RTP header !!!");
    }
    mLastRTPTime = rtpTime;
    getApartNTPNow(&mLastNTPSec, &mLastNTPUSec);

    return NO_ERROR;
}

status_t RTPTransport::receive(sp<MetaBuffer> &buffer)
{
    buffer = MetaBuffer::create(MaxCapacity);

    if (symmRTP) {
        udpTransport::receive(buffer, &mRTPAddr);
    } else {
        udpTransport::receive(buffer, NULL);
    }

    // TODO: deal with receive error and close connection
    // TODO: do statistic

    status_t err;

    err = parseRTP(buffer);
    if (err) {
        LOGV("packet parse error(%d bytes)", buffer->size());
    }

    return err;
}

void RTPTransport::notify(IRTPNotify::RTPMessage msg, uint32_t param)
{
    //F_LOG;
    LOGV("notify message %d %d", msg, param);
    IRTPNotify *observer = mObserver.get();
    if (observer != NULL) {
        observer->notify(msg, param);
    }
}

status_t RTPTransport::parseRTP(const sp<MetaBuffer> &buffer)
{
    ++mInfo.rcvdRTP;

    if (mInfo.rcvdRTP % 1000 == 0)
        LOGV("Packet in");

    size_t size = buffer->size();
    status_t err = UNKNOWN_ERROR;

    do
    {
        BREAK_IF(size < 12); // Too short to be a valid RTP header.

        const uint8_t *data = buffer->data();
        BREAK_IF((data[0] >> 6) != 2); // Unsupported version.

        if (data[0] & 0x20) {
            // Padding present.
            size_t paddingLength = data[size - 1];
            BREAK_IF(paddingLength + 12 > size);
            // If we removed this much padding we'd end up with something
            // that's too short to be a valid RTP header.

            size -= paddingLength;
        }

        int numCSRCs = data[0] & 0x0f;
        size_t payloadOffset = 12 + 4 * numCSRCs;
        BREAK_IF(size < payloadOffset);
          // Not enough data to fit the basic header and all the CSRC entries.

        if (data[0] & 0x10) {
            // Header eXtension present.

            BREAK_IF(size < payloadOffset + 4);
            // Not enough data to fit the basic header, all CSRC entries
            // and the first 4 bytes of the extension header.

            const uint8_t *extensionData = &data[payloadOffset];
            size_t extensionLength =
                4 * (extensionData[2] << 8 | extensionData[3]);
            BREAK_IF(size < payloadOffset + 4 + extensionLength);

            payloadOffset += 4 + extensionLength;
        }

        mInfo.rcvdSourceId = u32at(&data[8]);
        uint32_t rtpTime = u32at(&data[4]);

        buffer->setInt32("ssrc", mInfo.rcvdSourceId);
        buffer->setInt32("rtp-time", rtpTime);
        buffer->setInt32("PT", data[1] & 0x7f);
        buffer->setInt32("M", data[1] >> 7);

        //checkPacketLoss(u16at(&data[2]));

        buffer->setQuickData(u16at(&data[2]));
        buffer->setRange(payloadOffset, size - payloadOffset);
        if (firstRTP) {
            notify(IRTPNotify::RTPMSG_FIRST_PACKET, rtpTime);
            firstRTP = false;
        }

        //notify(IRTPNotify::RTPMSG_PACKET_DATA, (uint32_t)buffer.get());

        err = OK;
    }while(0);

    return err;
}

/// Actually, it is a reflected method for RTCP
void RTPTransport::handleRTCP()
{
    //F_LOG;
    status_t err;
    sp<MetaBuffer> buffer = MetaBuffer::create(MaxCapacity);
    //Mutex::Autolock _l(mLock);

    if (mRTCP == NULL) {
        LOGE("no recevie port");
        return;
    }

    if (symmRTP) {
        mRTCP->receive(buffer, &mRTCPAddr);
    } else {
        mRTCP->receive(buffer, NULL);
    }

    parseRTCP(buffer);
}

int RTPTransport::handleTimeout()
{
    //F_LOG;
    // send RTCP
    uint32_t _buffer[22];
    sp<MetaBuffer> _abuf = MetaBuffer::create(sizeof(_buffer), _buffer);

    {
        uint8_t * pHeader = (uint8_t*) _buffer;

        pHeader[0] = 0x81;  // one reception report assumed
        pHeader[1] = 200;  // SR
        pHeader[2] = 0;
        pHeader[3] = 12;   // a sender info plus a reception report occupied 52(13*4)bytes
        pHeader[4] = mSourceID >> 24;
        pHeader[5] = (mSourceID >> 16) & 0xff;
        pHeader[6] = (mSourceID >> 8) & 0xff;
        pHeader[7] = mSourceID & 0xff;
    }

    {
        uint32_t *pSender = &_buffer[2];
        pSender[0] = htonl(mLastNTPSec);
        pSender[1] = htonl(mLastNTPUSec);
        pSender[2] = htonl(mLastRTPTime);
        pSender[3] = htonl(mInfo.nRTPSent);
        pSender[4] = htonl(mInfo.nRTPOctetsSent);
    }

    {
        uint32_t *pReport = &_buffer[7];
        uint32_t _delay = minusNTPDelay(mInfo.lastSR);

        mInfo.totalRcvdLost += mInfo.nRcvdLost;
        if (mInfo.totalRcvdLost > 0x00FFFFFF) { // 24-bit
            mInfo.totalRcvdLost = 0x00FFFFFF;
        }
        if (mInfo.nRcvdLost > 255) {  // 8-bit
            mInfo.nRcvdLost = 255;
        }
        pReport[0] = htonl(mInfo.rcvdSourceId);
        pReport[1] = htonl(mInfo.nRcvdLost << 24 | mInfo.totalRcvdLost);
        pReport[2] = htonl(mInfo.rcvdMaxSeqNum);
        pReport[3] = 0; // TODO: measure jitter
        if (mInfo.lastSR) {
            pReport[4] = htonl((uint32_t)(mInfo.lastSR >> 16));
            pReport[5] = htonl(_delay);
        } else {
            pReport[4] = 0;
            pReport[5] = 0;
        }
        mInfo.nRcvdLost = 0;
    }
    /// Set SDES
    /// uint32_t *pSrcDesc = &_buffer[13];
    {
        uint8_t * pHeader = (uint8_t*)&_buffer[13];

        pHeader[0] = 0x81;  // one reception report assumed
        pHeader[1] = 202;  // SR
        pHeader[2] = 0;
        pHeader[3] = 6;   // a sender info plus a reception report occupied 28(7*4)bytes
        pHeader[4] = mSourceID >> 24;
        pHeader[5] = (mSourceID >> 16) & 0xff;
        pHeader[6] = (mSourceID >> 8) & 0xff;
        pHeader[7] = mSourceID & 0xff;
    }

    {
        uint8_t *pSender =(uint8_t *)&_buffer[15];
        pSender[0] = 1;
        pSender[1] = 16;
        snprintf((char*)&pSender[2], 17, "%s@", inet_ntoa(mHostAddr.sin_addr));
        pSender[18] = 0;
        pSender[19] = 0;
    }


    if (_abuf != NULL) {
        _abuf->setRange(0, 20*sizeof(uint32_t)); // SDES not included
        mRTCP->deliver(_abuf, mRTCPAddr);
    }

    // schdule next timeout
    mEventId = SocketPoller::Instance()->AddTimedCallback(RTCP_PERIOD, &mTimer);
    return 0;
}

status_t RTPTransport::parseRTCP(const sp<MetaBuffer> &buffer)
{
    if (mInfo.rcvdRTCP++ == 0) {
        notify(IRTPNotify::RTCPMSG_FIRST_PACKET, 0);
    }

    const uint8_t *data = buffer->data();
    size_t size = buffer->size();

    while (size > 0) {
        if (size < 8) {
            // Too short to be a valid RTCP header
            return -1;
        }

        if ((data[0] >> 6) != 2) {
            // Unsupported version.
            return -1;
        }

        if (data[0] & 0x20) {
            // Padding present.

            size_t paddingLength = data[size - 1];

            if (paddingLength + 8 > size) {
                // If we removed this much padding we'd end up with something
                // that's too short to be a valid RTCP header.
                return -1;
            }

            size -= paddingLength;
        }

        size_t headerLength = 4 * (data[2] << 8 | data[3]) + 4;

        if (size < headerLength) {
            // Only received a partial packet?
            return -1;
        }

        switch (data[1]) {
            case 200:
            {
                parseSR(data, headerLength);
                break;
            }

            case 201:  // RR
            case 202:  // SDES
            case 204:  // APP
                break;

            case 205:  // TSFB (transport layer specific feedback)
            case 206:  // PSFB (payload specific feedback)
                // hexdump(data, headerLength);
                break;

            case 203:
            {
                parseBYE(data, headerLength);
                break;
            }

            default:
            {
                LOGW("Unknown RTCP packet type %u of size %d",
                     (unsigned)data[1], headerLength);
                break;
            }
        }

        data += headerLength;
        size -= headerLength;
    }

    return OK;
}

status_t RTPTransport::parseBYE(const uint8_t *data, size_t size)
{
    size_t SC = data[0] & 0x3f;

    if (SC == 0 || size < (4 + SC * 4)) {
        // Packet too short for the minimal BYE header.
        return -1;
    }

    uint32_t id = u32at(&data[4]);

    notify(IRTPNotify::RTCPMSG_END_OF_STREAM, 0);
    //mSource->byeReceived();

    return OK;
}

status_t RTPTransport::parseSR(const uint8_t *data, size_t size)
{
    getWholeNTPNow(&mInfo.lastSR);
    size_t RC = data[0] & 0x1f;

    if (size < (7 + RC * 6) * 4) {
        // Packet too short for the minimal SR header.
        return -1;
    }

    IRTPNotify::timeInfo _info;
    uint32_t id = u32at(&data[4]);
    _info.ntpTime = u64at(&data[8]);
    _info.rtpTime = u32at(&data[16]);

    notify(IRTPNotify::RTCPMSG_TIME_UPDATE, (uint32_t)&_info);
#if 0
    LOGI("XXX timeUpdate: ssrc=0x%08x, rtpTime %u == ntpTime %.3f",
         id,
         rtpTime,
         (ntpTime >> 32) + (double)(ntpTime & 0xffffffff) / (1ll << 32));


    if ((mFlags & kFakeTimestamps) == 0) {
        mSource->timeUpdate(rtpTime, ntpTime);
    }
#endif
    return 0;
}

status_t RTPTransport::getStatistic(int &recvBytes, int &sendBytes,
                          int &recvPackets, int &sendPackets,
                          int &lostPackets)
{
    recvBytes = sendBytes = recvPackets = sendPackets = 0;
    return NO_ERROR;
}


