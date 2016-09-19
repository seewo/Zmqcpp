/*******************************************************************
*  Copyright(c) 2000-2016 Guangzhou Shirui Electronics Co., Ltd.
*  All rights reserved.
*
*  FileName:		Socket
*  Author:			libin
*  Date:			2016/01/05
*  Description:	
******************************************************************/

#ifndef __796D396EEC2C4988B6AC785EC12A8555_H__
#define __796D396EEC2C4988B6AC785EC12A8555_H__

#include <string>
#include <boost/noncopyable.hpp>
#include "ZmqConst.h"
#include "Context.h"

ZMQ_CPP_NAMESPACE_BEGIN

class Socket : public boost::noncopyable
{
public:
	enum EnumSocketType
	{
		REQ = ZMQ_REQ,
		REP = ZMQ_REP,
		DEALER = ZMQ_DEALER,
		ROUTER = ZMQ_ROUTER,
		PUB = ZMQ_PUB,
		SUB = ZMQ_SUB,
		XPUB = ZMQ_XPUB,
		XSUB = ZMQ_XSUB,
		PUSH = ZMQ_PUSH,
		PULL = ZMQ_PULL,
		PAIR = ZMQ_PAIR,
		STREAM = ZMQ_STREAM
	};

	explicit Socket(Context &ctx, EnumSocketType type)
		: m_context(ctx)
		, m_pSocket(NULL)
		, m_type(type)
	{

	}

	~Socket()
	{
		Close();
	}


	bool Close()
	{
		if (!GetSocket())
		{
			return false;
		}
		int rc = 0;
		if (m_pSocket)
		{
			rc = zmq_close(m_pSocket);
			m_pSocket = NULL;
		}
		return rc == 0;
	}

	void *GetSocket()
	{
		if (!NewSocket())
		{
			return NULL;
		}
		return m_pSocket;
	}
	
	bool NewSocket()
	{
		if (!m_pSocket)
		{
			m_pSocket = zmq_socket(m_context.GetContext(), static_cast<int>(m_type));
			if (!m_pSocket)
			{
				return false;
			}
		}
		return true;
	}

	bool Bind(const std::string &addr)
	{
		if (!GetSocket())
		{
			return false;
		}
		m_address = addr;
		int rc = zmq_bind(m_pSocket, addr.c_str());
		return rc == 0;
	}

	bool Unbind(const std::string &addr)
	{
		if (!GetSocket())
		{
			return false;
		}
		m_address = addr;
		int rc = zmq_unbind(m_pSocket, m_address.c_str());
		return rc == 0;
	}

	bool  Unbind()
	{
		if (!GetSocket())
		{
			return false;
		}
		int rc = zmq_unbind(m_pSocket, m_address.c_str());
		return rc == 0;
	}

	bool Connect(const std::string &addr)
	{
		if (!GetSocket())
		{
			return false;
		}
		m_address = addr;
		
		int rc = zmq_connect(m_pSocket, m_address.c_str());
		return rc == 0;

	}

	bool Disconnect()
	{
		if (!GetSocket())
		{
			return false;
		}
		int rc = zmq_disconnect(m_pSocket, m_address.c_str());
		return rc == 0;
	}

	bool Disconnect(const std::string & addr)
	{
		if (!GetSocket())
		{
			return false;
		}
		m_address = addr;
		int rc = zmq_disconnect(m_pSocket, m_address.c_str());
		return rc == 0;
	}

	bool Send(const void *buf, size_t len, int flags, int &sendSize)
	{
		if (!GetSocket())
		{
			return false;
		}
		int nbytes = zmq_send(m_pSocket, buf, len, flags);
		if (nbytes >= 0)
		{
			sendSize = nbytes;
			return true;
		}

		if (zmq_errno() == EAGAIN)
		{
			sendSize = 0;
			return true;
		}
		return false;
	}

	bool Sendiov(struct iovec iov[], size_t iovSize, int flags)
	{
		if (!GetSocket())
		{
			return false;
		}
		return (zmq_sendiov(m_pSocket, iov, iovSize, flags) != -1);
	}

	bool Recv(void *buf, size_t len, int flags, int &value)
	{
		if (!GetSocket())
		{
			return false;
		}
		int nbytes = zmq_recv(m_pSocket, buf, len, flags);
		if (nbytes >= 0)
		{
			value = nbytes;
			return true;
		}
		if (zmq_errno() == EAGAIN)
		{
			value = 0;
			return true;
		}
		return false;
	}

	bool Recviov(struct iovec *iov, size_t *iovSize, int flags)
	{
		if (!GetSocket())
		{
			return false;
		}
		return (zmq_recviov(m_pSocket, iov, iovSize, flags) != -1);
	}

	bool SetSocketOption(int option, const void *optval, size_t optvallen)
	{
		if (!GetSocket())
		{
			return false;
		}
		int rc = zmq_setsockopt(m_pSocket, option, optval, optvallen);
		return rc == 0;
	}

	template<typename T> 
	bool SetSocketOption(int option, T const& optval)
	{
		if (!GetSocket())
		{
			return false;
		}
		return SetSocketOption(option, &optval, sizeof(T));
	}

	bool GetSocketOption(int option, void *optval, size_t *optvallen)
	{
		if (!GetSocket())
		{
			return false;
		}
		int rc = zmq_getsockopt(m_pSocket, option, optval, optvallen);
		return rc == 0;
	}

	template<typename T> 
	bool GetSocketOption(int option, T &value)
	{
		if (!GetSocket())
		{
			return false;
		}
		size_t optlen = sizeof(T);
		return GetSocketOption(option, &value, &optlen);
	}

private:
	std::string m_address;
	void *m_pSocket;
	Context &m_context;
	EnumSocketType m_type;
};

ZMQ_CPP_NAMESPACE_END

#endif // __796D396EEC2C4988B6AC785EC12A8555_H_
