#ifndef __AST_INTF_H__
#define __AST_INTF_H__



class AstIntf : public Thread
{
public:
	static AstIntf* Instance();
	virtual ~AstIntf();
private:

};

#endif//__AST_INTF_H__
