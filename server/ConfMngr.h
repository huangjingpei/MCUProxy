#ifndef __CONF_MNGR_H__
#define __CONF_MNGR_H__

#include "MsgProto.h"
#include "ChannelMngr.h"
using namespace std;
class ConfMngr {
	status_t processRequest(AstRequestMsg *msg);
	sp<ChannelMngr> getChannelMngr();
	void freeChannelMngr(sp<ChannelMngr> mngr);
	std::map<char *string, sp<ChannelMngr>channelMngr > mConfMngr;
};
#endif//__CONF_MNGR_H__
