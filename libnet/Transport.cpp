#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>

#include "Transport.h"

#define LOG_TAG "Transport"
#include "utility.h"



Transport::Transport()
    : mSocket(-1)
    , mCallback(NULL)
{
}

Transport::~Transport()
{
    doClose();
}

status_t Transport::doClose()
{
    //F_LOG;
    status_t ret = NO_ERROR;

    do
    {
        ret = NO_INIT;
        SocketPoller *poller = SocketPoller::Instance();
        BREAK_IF(poller == NULL);

        ret = poller->CloseTransport(this);
        //ret = NO_ERROR;
    }while(0);

    return ret;
}

void Transport::handleClose()
{
    //F_LOG;
    Mutex::Autolock _l(mCallLock);
    if (mCallback) mCallback->handleClose();
}

void Transport::handleFree()
{
    //F_LOG;
}

void Transport::handleRead()
{
    //F_LOG;
    Mutex::Autolock _l(mCallLock);
    if (mCallback) mCallback->handleRead();
}

void Transport::handleWrite()
{
    //F_LOG;
}

status_t Transport::init()
{
    //F_LOG;
    status_t ret = NO_INIT;

    do
    {
        BREAK_IF(mSocket == -1);

        ITransport::sock = mSocket;
        SocketPoller *poller = SocketPoller::Instance();
        BREAK_IF(poller == NULL);

        ret = poller->AddTransport(this);
        BREAK_IF(ret != NO_ERROR);

    }while(0);

    return ret;
}

status_t Transport::setHandler(INetHandler *callback)
{
    //F_LOG;

    Mutex::Autolock _l(mCallLock);
    mCallback = callback;

    return NO_ERROR;
}

status_t Transport::setTypeOfService(int ipTOS) // gs_phone use this one
{
    status_t ret = UNKNOWN_ERROR;

    do
    {
        BREAK_IF(mSocket < 0);

        ret = setsockopt(mSocket, IPPROTO_IP, IP_TOS, &ipTOS, sizeof(ipTOS));
    }while(0);

    if (ret != NO_ERROR) {
        LOGE("set TOS ret = %d errno = %d", ret, errno);
    }

    return ret;
}

status_t Transport::setVLanPriority(int pri)
{
    status_t ret = UNKNOWN_ERROR;

    do
    {
        BREAK_IF(mSocket < 0);

        ret = setsockopt(mSocket, SOL_SOCKET, SO_PRIORITY, &pri, sizeof(pri));
    }while(0);

    if (ret != NO_ERROR) {
        LOGE("set PRI ret = %d errno = %d", ret, errno);
    }

    return ret;
}

status_t udpTransport::init()
{
    //F_LOG;
    status_t ret = NO_ERROR;

    do
    {
        if (mSocket > 2) {
            LOGI("socket %d has been initilized.", mSocket);
            break;
        }

        ret = UNKNOWN_ERROR;
        mSocket = socket(AF_INET, SOCK_DGRAM, 0);
        BREAK_IF(mSocket < 0);

        /*{
            // Make socket non-blocking.
            int flags = fcntl(Transport::sock, F_GETFL, 0);
            BREAK_IF(flags == -1);

            flags |= O_NONBLOCK;
            ret = fcntl(Transport::sock, F_SETFL, flags);
            BREAK_IF(ret < 0);
        }*/

        mStunRequested = mStunResponsed = false;

        ret = Transport::init(); //NO_ERROR;
    }while(0);

    // fail case flow
    if (ret != NO_ERROR && mSocket > 2) {
        close(mSocket);
        mSocket = -1;
    }

    return ret;
}

status_t udpTransport::bindAddress(sockaddr_in _addr, bool reuse)
{
    //F_LOG;
    status_t ret = UNKNOWN_ERROR;

    do
    {
        BREAK_IF(mSocket < 0);

        if (reuse) {
            int optval = 1;
            //setsockopt(mSocket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
            setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        }
        ret = bind(mSocket, (const struct sockaddr *)&_addr, sizeof(_addr));
        BREAK_IF(ret < 0);

        mBindAddr = _addr;
        mStunRequested = mStunResponsed = false;
        memset(&mReflAddr, 0, sizeof(mReflAddr));

        ret = NO_ERROR;
    }while(0);

    if (ret != NO_ERROR) {
        LOGE("ret = %d errno = %d", ret, errno);
    }

    return ret;
}

status_t udpTransport::connectTo(sockaddr_in _addr)
{
    status_t ret = UNKNOWN_ERROR;

    do
    {
        BREAK_IF(mSocket < 0);

        ret = connect(mSocket,
                  (const struct sockaddr*)&_addr, sizeof(_addr));

     }while(0);

    return ret;
}

status_t udpTransport::joinGroup(sockaddr_in _addr)
{
    status_t ret = UNKNOWN_ERROR;
    struct ip_mreq mreq;

    do
    {
        BREAK_IF(mSocket < 0);

        ret = bind(mSocket, (const struct sockaddr *)&_addr, sizeof(_addr));
        BREAK_IF(ret < 0);

        memcpy(&mreq.imr_multiaddr, &_addr.sin_addr, sizeof(in_addr));

        {
            struct ifreq ifreq;

            strncpy(ifreq.ifr_name, "eth0", IFNAMSIZ);
            ret = ioctl(mSocket, SIOCGIFADDR, &ifreq);
            BREAK_IF(ret < 0);

            memcpy(&mreq.imr_interface,
                   &((struct sockaddr_in *) &ifreq.ifr_addr)->sin_addr,
                   sizeof(struct in_addr));
        }

        ret = setsockopt(mSocket, SOL_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
        BREAK_IF(ret < 0);

        mBindAddr = _addr;
        ret = NO_ERROR;
    }while(0);

    return ret;
}

status_t udpTransport::sendStunRequest(sockaddr_in _addr)
{
    F_LOG;

    status_t err = UNKNOWN_ERROR;

    if (mStunRequested) { // not a very critical case, so mutex is not used
        return NO_ERROR;
    }

    mStunMessage.method = htons(0x0001);
    mStunMessage.length = 0;
    mStunMessage.magic = htonl(0x2112A442);// magic cookie
    mStunMessage.transId[0] = rand();
    mStunMessage.transId[1] = rand();
    mStunMessage.transId[2] = rand();

    ssize_t n = sendto(mSocket, &mStunMessage, sizeof(mStunMessage), 0,
                       (const struct sockaddr *)&_addr, sizeof(_addr));

    if (n < sizeof(_addr)) {
        LOGE("stun bind failed. (err=%d)", errno);
        return UNKNOWN_ERROR;
    }

    mStunRequested = true;
    mStunResponsed = false;
    return NO_ERROR;
}

status_t udpTransport::dropStunParsing()
{
    mStunRequested = mStunResponsed = false;
    memset(&mReflAddr, 0, sizeof(mReflAddr));
    return NO_ERROR;
}

status_t udpTransport::getStunReflect(sockaddr_in &_addr)
{
    if (mStunResponsed) {
        _addr = mReflAddr;
        return NO_ERROR;
    } else {
        return NO_INIT;
    }
}

void udpTransport::parseStunResponse(sp<MetaBuffer> &buffer)
{
    struct stunMessage *pMsg;

    struct _attr_hdr
    {
        uint16_t attr;
        uint16_t len;
    }*pAttrHdr;

    struct _attr_ip
    {
        uint8_t reserved;
        uint8_t family;
        uint16_t port;
        uint32_t ipv4;
    }*pAttrIP;

    do
    {
        DOING_IF(mStunRequested);
        DOING_IF(!mStunResponsed);

        pMsg = (stunMessage *)buffer->data();
        DOING_IF(ntohs(pMsg->method) == 0x0101);
        DOING_IF(ntohl(pMsg->magic) == 0x2112A442);
        //BREAK_IF(pMsg->transId[0] != mStunMessage.transId[0]);
        //BREAK_IF(pMsg->transId[1] != mStunMessage.transId[1]);
        //BREAK_IF(pMsg->transId[2] != mStunMessage.transId[2]);
        void * _cursor = (void*)pMsg + sizeof(stunMessage);
        uint16_t _length = ntohs(pMsg->length);
        pAttrIP = NULL;

        while (((_length & 0x3) == 0) &&
               (_length >= sizeof(struct _attr_hdr)))
        {
            pAttrHdr = (_attr_hdr*)_cursor;
            pAttrHdr->len = ntohs(pAttrHdr->len);
            //EVAL_INT(pAttrHdr->len);
            BREAK_IF(pAttrHdr->len & 0x3); // wrong attribute
            DOING_IF(pAttrHdr->len == sizeof(struct _attr_ip));
            if (ntohs(pAttrHdr->attr) == 0x0001) { // MAPPED-ADDRESS
                pAttrIP = (_attr_ip *)(_cursor + sizeof(_attr_hdr));
                // need to check family?
                break;
            }

            _cursor += pAttrHdr->len;
            _length -= pAttrHdr->len;
        }

        DOING_IF(pAttrIP != NULL);

        mReflAddr.sin_family = AF_INET;
        mReflAddr.sin_port = pAttrIP->port;
        mReflAddr.sin_addr.s_addr = pAttrIP->ipv4;

        LOGV("stun reflective address is %s:%u",
             inet_ntoa(mReflAddr.sin_addr), ntohs(mReflAddr.sin_port));

        ///buffer->setRange(0, 0); // consumed
        mStunResponsed = true;
    }while(0);

    return;
}

status_t udpTransport::deliver(const sp<MetaBuffer> &buffer, sockaddr_in _addr)
{
    ssize_t n = sendto(
            mSocket, buffer->data(), buffer->size(), 0,
            (const struct sockaddr *)&_addr, sizeof(_addr));

    //CHECK_EQ(n, (ssize_t)buffer->size());
    if(n < (ssize_t)buffer->size() ){
        LOGE("RTP Send failed. %d/%d(err=%d)", n, buffer->size(), errno);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t udpTransport::receive(sp<MetaBuffer> &buffer, sockaddr_in *peerAddr)
{
    socklen_t remoteAddrLen = sizeof(mPeerAddr);

    //LOGV("recvfrom with socket %d", s->mRTPSocket);
    ssize_t nbytes = recvfrom(mSocket,
            buffer->data(),
            buffer->capacity(),
            0,
            (struct sockaddr *)&mPeerAddr,
            &remoteAddrLen);

    if (nbytes < 0) {
        return UNKNOWN_ERROR;
    } else if (nbytes == 0) {
        return DEAD_OBJECT;
    }

    buffer->setRange(0, nbytes);
    if (peerAddr != NULL) {
        *peerAddr = mPeerAddr;
    }

    if (mStunRequested && !mStunResponsed) {
        parseStunResponse(buffer);
    }
    return NO_ERROR;
}

status_t udpTransport::getHostAddr(sockaddr_in&  _addr)
{
    status_t ret = UNKNOWN_ERROR;

    do
    {
        BREAK_IF(mSocket < 0);

        struct ifconf conf;
        struct ifreq *ifr;
        char buff[128];

        conf.ifc_len = 128;
        conf.ifc_buf = buff;

        ioctl(mSocket, SIOCGIFCONF, &conf);
        ifr = conf.ifc_req;
        ifr++;

        _addr = *((struct sockaddr_in *)(&ifr->ifr_addr));

        ret = NO_ERROR;
    }while(0);

    return ret;
}


