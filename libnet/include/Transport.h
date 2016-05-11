#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <netinet/in.h>
#include "Poller.h"

#define  REFLECT_READ_HANDLER(cls, var, method)   \
    class NetHandler : public INetHandler { \
    public: \
        NetHandler(cls *target) \
            : mTarget(target) { \
        } \
        virtual void handleClose() { \
        } \
        virtual void handleRead() { \
            if (mTarget) { \
                mTarget->method(); \
            } \
        } \
    private: \
        cls *mTarget; \
        NetHandler(const NetHandler &); \
        NetHandler &operator=(const NetHandler &); \
    }; \
    friend class NetHandler; \
    NetHandler var; \
    void method();

using namespace android;

struct INetHandler
{
    INetHandler() {};
    virtual ~INetHandler() {};

    virtual void handleRead() = 0;
    virtual void handleClose() = 0;
};

class Transport : public ITransport
{
public:
    Transport();
    virtual ~Transport();

    virtual status_t init();
    virtual status_t setHandler(INetHandler *callback);

    status_t setTypeOfService(int ipTOS); // gs_phone use this one
    status_t setVLanPriority(int pri);

    // ITransport interface
    virtual void handleClose();
    virtual void handleFree();
    virtual void handleRead();
    virtual void handleWrite();
protected:
    int mSocket;

    virtual status_t doClose();

private:
    Mutex mCallLock;
    INetHandler *mCallback;

};

class udpTransport : public Transport
{
public:
    //udpTransport();
    virtual ~udpTransport() {};

    virtual status_t init();
    status_t bindAddress(sockaddr_in _addr, bool reuse=true);
    status_t connectTo(sockaddr_in _addr);
    status_t joinGroup(sockaddr_in _addr);

    status_t deliver(const sp<MetaBuffer> &buffer, sockaddr_in _addr);
    status_t receive(sp<MetaBuffer> &buffer, sockaddr_in *peerAddr=NULL);

    status_t sendStunRequest(sockaddr_in _addr);
    status_t dropStunParsing();
    status_t getStunReflect(sockaddr_in &_addr);
    status_t getHostAddr(sockaddr_in &_addr);
private:
    struct sockaddr_in mPeerAddr;
    struct sockaddr_in mBindAddr;

    struct stunMessage
    {
        uint16_t method;
        uint16_t length;
        uint32_t magic;
        uint32_t transId[3];
    }mStunMessage;

    bool mStunResponsed;
    bool mStunRequested;
    struct sockaddr_in mReflAddr;

    void parseStunResponse(sp<MetaBuffer> &buffer);
};
#if 0
class tcpTransport : public Transport
{
public:
    tcpTransport();
    virtual ~tcpTransport() {};

    int init(int listenfd);
private:
    static int warppedRead(epTransport *trans);
    static int warppedClose(epTransport *trans);
    static int warppedFree(epTransport *trans);

    static int peerIdle(tcpTransport *self);

    int m_sock;
    int m_idleTimer;
    bool m_isVerified;

    epVirtualTable m_tcpTransTable;
};
#endif
#endif

