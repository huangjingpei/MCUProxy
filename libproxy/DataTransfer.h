#ifndef __DATA_TRANSFER_H__
#define __DATA_TRANSFER_H__

#include "UDPSender.h"
#include "UDPRecver.h"
class DataTransfer {
public:
	static DataTransfer *create();
	virtual ~DataTransfer();
private:
	DataTransfer();

	sp<UDPSender> rtpSender;
	sp<UDPSender> rtcpSender;
	sp<UDPRecver> rtpRecver;
	sp<UDPRecver> rtcpRecver;
};
#endif//__DATA_TRANSFER_H__
