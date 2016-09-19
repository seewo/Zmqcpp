/*******************************************************************
*  Copyright(c) 2000-2016 Guangzhou Shirui Electronics Co., Ltd.
*  All rights reserved.
*
*  FileName:		ZmqClient
*  Author:			libin
*  Date:			2016/01/05
*  Description:	
******************************************************************/

#ifndef __1B6C42F8F8404FBD9209AAD3D8206F1E_H__
#define __1B6C42F8F8404FBD9209AAD3D8206F1E_H__

#include "ZmqClientBase.h"

class ZmqClientDealer : public ZmqClientBase
{
public:
	ZmqClientDealer() : ZmqClientBase(ZmqClientBase::DEALER) {}
	~ZmqClientDealer() {}

	bool SendDataAndCommand(const std::string &message, const std::string command = std::string())
	{
		bool rc = SendData(command, ZMQ_SNDMORE);
		if (!rc)
		{
			return false;
		}
		rc = SendData(message, 0);
		if (!rc)
		{
			return false;
		}
		return true;
	}

	bool SendDataAndCommand(const char *data, int dataSize, const char *command = "", int commandSize = 0)
	{
		bool rc = SendData(command, commandSize, ZMQ_SNDMORE);
		if (!rc)
		{
			return false;
		}
		rc = SendData(data, dataSize, 0);
		if (!rc)
		{
			return false;
		}
		return true;
	}
private:
	virtual void BackSocketRecv(zmqcpp::Socket &backsock, zmqcpp::Socket &clisock)
	{
		iovec vec[IOVET_SIZE] = { 0 };
		size_t count = IOVET_SIZE;

		if (backsock.Recviov(vec, &count, 0))
		{
			int sendSize = 0;
			for (size_t i = 0; i < count; ++i)
			{
				if (i == (count - 1))
				{
					clisock.Send(vec[i].iov_base, vec[i].iov_len, 0, sendSize);
				}
				else
				{
					clisock.Send(vec[i].iov_base, vec[i].iov_len, ZMQ_SNDMORE, sendSize);
				}
			}
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

#endif // __1B6C42F8F8404FBD9209AAD3D8206F1E_H_
