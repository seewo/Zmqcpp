/*******************************************************************
*  Copyright(c) 2000-2016 Guangzhou Shirui Electronics Co., Ltd.
*  All rights reserved.
*
*  FileName:		ZmqStream
*  Author:			libin
*  Date:			2016/01/08
*  Description:	基于ZEROMQ 与 普通SOCKET通信的封装
******************************************************************/

#ifndef __C1CC2759ED0C4600815F4BD1307862DC_H__
#define __C1CC2759ED0C4600815F4BD1307862DC_H__

#include "ZmqClientBase.h"

class ZmqClientStream : public ZmqClientBase
{
public:
	ZmqClientStream() : ZmqClientBase(ZmqClientBase::STREAM) {}
	~ZmqClientStream() {}

	bool Send(const std::string &data)
	{
		return SendData(data, 0);
	}

	bool Send(const char *data, int dataSize)
	{
		return SendData(data, dataSize, 0);
	}

private:
	virtual void BackSocketRecv(zmqcpp::Socket &backsock, zmqcpp::Socket &clisock)
	{
		iovec vec[IOVET_SIZE] = { 0 };
		size_t count = IOVET_SIZE;

		if (backsock.Recviov(vec, &count, 0))
		{
			bool rc = 0;
			int sendSize = 0;
			char id[256];
			size_t idsize = 256;

			if (!clisock.GetSocketOption(ZMQ_IDENTITY, id, &idsize))
			{
				GLERROR << "error, BackSocketRecv GetSocketOption ZMQ_IDENTITY" << ", code=" << zmq_strerror(zmq_errno());
				return;
			}
			rc = clisock.Send(id, idsize, ZMQ_SNDMORE, sendSize);
			rc = clisock.Sendiov(vec, count, 0);
		}

		for (size_t i = 0; i < count; ++i)
		{
			if (vec[i].iov_base != NULL)
			{
				free(vec[i].iov_base);
			}
		}
	}
};

#endif // ___H_
