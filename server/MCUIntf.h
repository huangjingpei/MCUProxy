#ifndef __MCU_INTF_H__
#define __MCU_INTF_H__

#include <MsgProto.h>
class MCUIntf {
public:
	static MCUIntf* Instance();
	virtual ~MCUIntf();
private:
	char *geneRequestMsg();

private:
	MCUIntf();
};
#endif//__MCU_INTF_H__
