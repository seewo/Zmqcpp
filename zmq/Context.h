/*******************************************************************
*  Copyright(c) 2000-2016 Guangzhou Shirui Electronics Co., Ltd.
*  All rights reserved.
*
*  FileName:		Context
*  Author:			libin
*  Date:			2016/01/05
*  Description:	
******************************************************************/

#ifndef __DB3296E7461D46858AF2031C01554D13_H__
#define __DB3296E7461D46858AF2031C01554D13_H__

#include <cassert>
#include <boost/noncopyable.hpp>
#include "ZmqConst.h"

ZMQ_CPP_NAMESPACE_BEGIN

class Context : public boost::noncopyable
{
public:
	Context() 
	: m_pContext(NULL)
	{
		m_pContext = zmq_ctx_new();
		assert(m_pContext);
	}

	explicit Context(int ioThreads, int maxSocket = ZMQ_MAX_SOCKETS_DFLT)
	{
		m_pContext = zmq_ctx_new();
		assert(m_pContext);

		int rc = zmq_ctx_set(m_pContext, ZMQ_IO_THREADS, ioThreads);
		assert(rc == 0);

		rc = zmq_ctx_set(m_pContext, ZMQ_MAX_SOCKETS, maxSocket);
		assert(rc == 0);
	}

	~Context() 
	{
		int rc = zmq_ctx_destroy(m_pContext);
		assert(rc == 0);
	}

	void Close()
	{
		int rc = zmq_ctx_shutdown(m_pContext);
		assert(rc == 0);
	}

	void *GetContext()
	{
		return m_pContext;
	}

	//const void *GetContext() const
	//{
	//	return m_pContext;
	//}

private:
	void *m_pContext;
};

ZMQ_CPP_NAMESPACE_END

#endif // __DB3296E7461D46858AF2031C01554D13_H_
