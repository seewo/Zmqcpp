/*******************************************************************
*  Copyright(c) 2000-2016 Guangzhou Shirui Electronics Co., Ltd.
*  All rights reserved.
*
*  FileName:		ZmqConst
*  Author:			libin
*  Date:			2016/01/05
*  Description:	
******************************************************************/

#ifndef __74566C9E7ADF434F9DA50F26DDC644F2_H__
#define __74566C9E7ADF434F9DA50F26DDC644F2_H__

#define ZMQ_STATIC
#include "zmq.h"

#define ZMQ_CPP_NAMESPACE_BEGIN namespace zmqcpp \
{
#define ZMQ_CPP_NAMESPACE_END \
}

#ifdef _WIN32
struct iovec {
	void *iov_base;
	size_t iov_len;
};
#else
#include <sys/uio.h>
#endif


ZMQ_CPP_NAMESPACE_BEGIN

#ifdef _WIN32
typedef DWORD Tick;
static Tick GetTick()
{
	return GetTickCount();
}
#else
typedef int Tick;
static Tick GetTick()
{
	return 0;
}
#endif // _WIN32

ZMQ_CPP_NAMESPACE_END

#endif // ___H_
