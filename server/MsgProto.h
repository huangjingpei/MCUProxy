#ifndef __MSG_PROTO_H__
#define __MSG_PROTO_H__
typedef enum _tag_AstRequstType{
	AST_REQ_START,
	AST_REQ_STOP,
	AST_REQ_SETPARAM
}AstRequstType;
typedef struct _tag_AstRequestMsg {
	AstRequstType msgType;
	char chanId[128];
	char confId[128];
	int msgId;
}AstRequestMsg;


typedef struct _tag_AstResponseMsg {
	AstRequstType msgType;
	char chanId[128];
	char confId[128];
	unsigned short audioPort;
	unsigned short videoPort;
	int msgId;
}AstResponseMsg;


#define MCURequestMsg	AstRequestMsg
#define MCUResponseMsg  AstResponseMsg

#endif//__MSG_PROTO_H__
