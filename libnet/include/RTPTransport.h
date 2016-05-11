#ifndef RTP_TRANSPORT_H
#define RTP_TRANSPORT_H

#include "Transport.h"
#include "absTimerList.h"

using namespace android;

class IRTPNotify : public RefBase
{
public:
    enum RTPMessage {
        RTPMSG_FIRST_PACKET,
        RTPMSG_PACKET_DATA,
        RTCPMSG_FIRST_PACKET,
        RTCPMSG_TIME_UPDATE,
        RTCPMSG_END_OF_STREAM,
    };

    struct timeInfo {
        uint32_t reserved;
        uint32_t rtpTime;
        uint64_t ntpTime;
    };
    virtual void notify(RTPMessage msg, uint32_t param) = 0;
};

class RTPTransport : public udpTransport
{
public:
    static RTPTransport *create(IRTPNotify *observer, bool rtcp=false);
    static const int MaxCapacity;

    virtual ~RTPTransport();

    virtual status_t init();
    virtual status_t setHandler(INetHandler *callback);
    status_t setObserver(IRTPNotify *observer);

    status_t setBindPort(uint16_t port);
    status_t setSourceID(uint32_t ssrc);
    status_t setPayloadType(int ptype);
    status_t setTarget(sockaddr_in &_addr, bool symmetric=false);

    status_t deliver(const sp<MetaBuffer> &buffer, int64_t timeUs,
                     bool video=true, bool marker=false);

    virtual status_t doSend(const sp<MetaBuffer> &buffer, int64_t timeUs,
                     bool video, bool marker);

    status_t receive(sp<MetaBuffer> &buffer);

    status_t getStatistic(int &recvBytes, int &sendBytes,
                          int &recvPackets, int &sendPackets,
                          int &lostPackets);

protected:
    void notify(IRTPNotify::RTPMessage msg, uint32_t param);

    status_t parseRTP(const sp<MetaBuffer> &buffer);
    status_t parseRTCP(const sp<MetaBuffer> &buffer);
    status_t parseSR(const uint8_t *data, size_t size);
    status_t parseBYE(const uint8_t *data, size_t size);

    REFLECT_READ_HANDLER(RTPTransport, mHandler, handleRTCP); // for RTCP read
    REFLECT_TIME_HANDLER(RTPTransport, mTimer); // for RTCP timer

    bool useRTCP;
    bool symmRTP;
    bool firstRTP;

    sp<IRTPNotify> mObserver;

    sp<udpTransport> mRTCP;
    int mRTCPSocket;
    int mEventId;

    struct sockaddr_in mRTPAddr;
    struct sockaddr_in mRTCPAddr;
    struct sockaddr_in mHostAddr;

    struct {
        uint32_t nRTPSent;
        uint32_t nRTPOctetsSent;

        uint32_t nRcvdLost; // TODO: should count on jitter buffer's output

        uint32_t rcvdSourceId;
        uint32_t rcvdMaxSeqNum;
        uint32_t rcvdRTP;
        uint32_t rcvdRTPOctets;
        uint32_t rcvdRTCP;

        uint32_t totalRcvdLost;
        uint32_t arrivalJitter;
        uint64_t lastSR;
    }mInfo;

    uint32_t mSourceID;
    uint32_t mSeqNo;
    uint32_t mRTPTimeBase;

    uint32_t mLastRTPTime;
    uint32_t mLastNTPSec;
    uint32_t mLastNTPUSec;

    uint8_t mPType;

private:
    //RTPTransport();
    RTPTransport(bool rtcp=false);
    RTPTransport(const RTPTransport&);
    void operator=(const RTPTransport&);
};

#endif

