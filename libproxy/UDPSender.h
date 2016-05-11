#ifndef __UDP_SENDER_H__
#define __UDP_SENDER_H__
class UDPSender {
public:
	static UDPSender* create();
	virtual ~UDPSender();
private:
	UDPSender();
};
#endif//__UDP_SENDER_H__
