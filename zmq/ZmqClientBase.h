/*******************************************************************
*  Copyright(c) 2000-2016 Guangzhou Shirui Electronics Co., Ltd.
*  All rights reserved.
*
*  FileName:		ZmqClientBase
*  Author:			libin
*  Date:			2016/01/12
*  Description:	
******************************************************************/

#ifndef __8DE54530D7F4459CAAD8F3994AA20712_H__
#define __8DE54530D7F4459CAAD8F3994AA20712_H__

#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <boost/atomic.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/placeholders.hpp>
#include <logger/Logger.h>
#include "TimerWorker.h"
#include "Context.h"
#include "Socket.h"
#include "Monitor.h"

class ZmqClientBase
{
public:
	enum EnumZmqClientType
	{
		DEALER,
		STREAM
	};

	enum EnumConnectState
	{
		ON_EVENT_CONNECTED,
		ON_EVENT_CONNECT_DELAYED,
		ON_EVENT_CONNECT_RETRIED,
		ON_EVENT_CLOSED,
		ON_EVENT_CLOSE_FAILED,
		ON_EVENT_DISCONNECTED,
		ON_EVENT_UNKNOWN
	};
	typedef boost::function<void(EnumConnectState)> ConnectStateFunc;
	typedef boost::function< void(struct iovec[], size_t) > RecvFunc;

	explicit ZmqClientBase(EnumZmqClientType clientType)
		: m_bRun(false)
		, m_transportState(UNDEFINE)
		, m_context(1)
		, m_bAutoConnect(true)
		, m_autoConnectTimer(5000)
		, m_heartbeatInterval(10 * 1000)
		, m_heartbeatDelay(5 * 1000)
		, m_clientType(clientType)
	{
		m_autoConnectTimer.SetWorkRoutine(boost::bind(&ZmqClientBase::AutoConnect, this));
	}

	virtual ~ZmqClientBase()
	{
		Reset();
	}

	// 重置
	bool Reset()
	{
		m_autoConnectTimer.Stop();
		Stop();
		return true;
	}

	//addr需要符合ZMQ格式要求， 重置初始化
	bool Reset(const std::string &addr, ConnectStateFunc connectStateFn, RecvFunc recvFn)
	{
		Stop();
		m_address = addr;
		return Init(connectStateFn, recvFn);
	}

	bool Reset(const std::string &ip, uint16_t port, ConnectStateFunc connectStateFn, RecvFunc recvFn)
	{
		Stop();
		GLINFO << "stop ok";
		std::stringstream ss;
		ss << "tcp://" << ip << ":" << port;
		m_address = ss.str();
		return Init(connectStateFn, recvFn);
	}

	//等于0的时候不自动重连，单位秒.设置好后，下次生效。
	void SetAutoConnect(bool bAutoConnect = true)
	{
		m_bAutoConnect = bAutoConnect;
		m_autoConnectTimer.Start();
	}

	//多长时间没有接收到回包就算是超时判断（heartbeatIntervalSeconds + heartbeatDelaySeconds）。
	//heartbeatIntervalSeconds等于0的时候不判断超时。
	//heartbeatIntervalSeconds 回包的时长，通常是心条
	//heartbeatDelaySeconds 超时允许再等待的时间。
	void SetHeartbeatIntervalTime(uint32_t heartbeatIntervalSeconds = 10, uint32_t heartbeatDelaySeconds = 5)
	{
		m_heartbeatInterval = heartbeatIntervalSeconds * 1000;
		m_heartbeatDelay = heartbeatDelaySeconds * 1000;
	}

	bool SendData(const std::string &data, int flags = 0)
	{
		int sendSize = 0;
		bool rc = SendData(data.data(), data.size(), flags, sendSize);
		if (!rc || (sendSize != data.size()))
		{
			return false;
		}
		return true;
	}

	bool SendData(const char *data, int dataSize, int flags = 0)
	{
		int sendSize = 0;
		bool rc = SendData(data, dataSize, flags, sendSize);
		if (!rc || !(sendSize != dataSize))
		{
			return false;
		}
		return true;
	}

protected:
	enum EnumTransportState
	{
		TRANSPORT_DISCONNECT = -2,
		TRANSPORT_FAILED = -1,
		UNDEFINE = 0,
		TRANSPORTED_POLL_FINISH = 1,
		TRANSPORTING = 2,
		TRANSPORTED = 3,
	};

	enum
	{
		POLL_MS = 200,
		IOVET_SIZE = 8
	};

	bool SendData(const void *buf, size_t len, int flags, int &sendSize)
	{
		if (m_transportState < TRANSPORTING)
		{
			return false;
		}
		boost::shared_ptr<zmqcpp::Socket> sock = NewFrontSockets();
		if (!sock || !sock->GetSocket())
		{
			return false;
		}
		return sock->Send(buf, len, flags, sendSize);
	}

	virtual void BackSocketRecv(zmqcpp::Socket &backsock, zmqcpp::Socket &clisock)
	{
		iovec vec[IOVET_SIZE] = { 0 };
		size_t count = IOVET_SIZE;

		if (backsock.Recviov(vec, &count, 0))
		{
			clisock.Sendiov(vec, count, 0);
		}

		for (size_t i = 0; i < count; ++i)
		{
			if (vec[i].iov_base != NULL)
			{
				free(vec[i].iov_base);
			}
		}
	}

private:
	void AutoConnect()
	{
		if (m_bAutoConnect)
		{
			int64_t value = -1;
			if (m_heartbeatInterval > 0)
			{
				zmqcpp::Tick nowtick = zmqcpp::GetTick();
				//判断超时，现在时间减去最后一次ping的时间，再减去心跳和心跳容忍时间。
				value = (int64_t)(nowtick - m_lastpong) - (int64_t)(m_heartbeatInterval + m_heartbeatDelay);
			}
			if (value > -1 || m_transportState < TRANSPORTING)
			{
				GLINFO << "reset connect, timeout ";
				ConnectStateFunc connectStateFunc = m_connectStateFunc;
				RecvFunc recvFunc = m_recvFunc;
				std::string address = m_address;
				Reset(address, connectStateFunc, recvFunc);
			}
		}
		else
		{
			m_autoConnectTimer.Stop();
		}
	}

	void RecvPoll()
	{
		m_bStopMonitor = false;
		m_recvPollThreadid = boost::this_thread::get_id();
		zmqcpp::Socket::EnumSocketType socketType;
		if (m_clientType == STREAM)
		{
			socketType = zmqcpp::Socket::STREAM;
		}
		else if (m_clientType == DEALER)
		{
			socketType = zmqcpp::Socket::DEALER;
		}
		else
		{
			m_transportState = TRANSPORT_FAILED;
			GLERROR << "error, unknown socket type";
			return;
		}

		zmqcpp::Socket m_clisock(m_context, socketType);
		zmqcpp::Socket m_backsock(m_context, zmqcpp::Socket::PULL);

		if (!m_clisock.NewSocket())
		{
			m_transportState = TRANSPORT_FAILED;
			GLERROR << "error, " << "new client socket" << ", code=" << zmq_strerror(zmq_errno());
			return;
		}

		if (!m_backsock.NewSocket())
		{
			m_transportState = TRANSPORT_FAILED;
			GLERROR << "error, " << "new back socket failed" << ", code=" << zmq_strerror(zmq_errno());
			return;
		}

		if (m_identity.empty())
		{
			boost::uuids::random_generator rgen;
			boost::uuids::uuid ranuuid = rgen(); // 生成一个随机uuid
			std::stringstream ss;
			ss << ranuuid;
			m_identity = ss.str();
		}

		zmqcpp::Monitor monitor;
		if (!SetMonitorSocket(monitor, m_clisock))
		{
			m_transportState = TRANSPORT_FAILED;
			GLERROR << "error, SetMonitorSocket" << ", code=" << zmq_strerror(zmq_errno());
			return;
		}

		if (!SetClientSocket(m_clisock))
		{
			m_transportState = TRANSPORT_FAILED;
			GLERROR << "error, SetClientSocket" << ", code=" << zmq_strerror(zmq_errno());
			return;
		}


		if (!SetBackSocket(m_backsock))
		{
			m_transportState = TRANSPORT_FAILED;
			GLERROR << "error, SetBackSocket" << ", code=" << zmq_strerror(zmq_errno());
			return;
		}

		zmq_pollitem_t item[] = {
			{ m_backsock.GetSocket(), 0, ZMQ_POLLIN, 0 },
			{ m_clisock.GetSocket(), 0, ZMQ_POLLIN, 0 },
			{ monitor.GetSocket(), 0, ZMQ_POLLIN, 0 }
		};

		m_bRun = true;
		int rc = 0;
		m_transportState = TRANSPORTING;

		GLINFO << "pool thread, start";
		while (m_bRun)
		{
			rc = zmq_poll(item, 3, POLL_MS);
			if (rc > 0)
			{
				if (item[0].revents & ZMQ_POLLIN)
				{
					BackSocketRecv(m_backsock, m_clisock);
				}
				if (item[1].revents & ZMQ_POLLIN)
				{
					ClientSocketRecv(m_clisock);
				}
				if (item[2].revents & ZMQ_POLLIN)
				{
					monitor.MonitorRecv();
				}
			}
		}

		m_backsock.Unbind();
		m_backsock.Close();
		m_clisock.Disconnect();
		m_clisock.Close();

		int timeoutCount = 0;
		while (!m_bStopMonitor)
		{
			monitor.MonitorRecv();
			rc = zmq_poll(0, 0, 10);
			if (++timeoutCount > 1000)
			{
				GLINFO << "wait over 10s, stop monitor, timeout";
				break;
			}
		}
		m_transportState = TRANSPORTED_POLL_FINISH;
		GLINFO << "poll thread, end, set m_transportState to TRANSPORTED_POLL_FINISH";
	}

	bool Init(ConnectStateFunc connectStateFn, RecvFunc recvFn)
	{
		if (m_recvthread)
		{
			if (m_recvthread->joinable())
			{
				GLERROR << "error, recv poll thread not null";
				return false;
			}
		}

		GLINFO << "update last pong";
		UpdateLastPong();

		if (m_bAutoConnect)
		{
			m_autoConnectTimer.Start();
		}

		m_connectStateFunc = connectStateFn;
		m_recvFunc = recvFn;
		m_transportState = UNDEFINE;
		m_recvthread.reset(new boost::thread(boost::bind(&ZmqClientBase::RecvPoll, this)));

		GLINFO << "reset thread ok";
		while (true)
		{
			if (m_transportState != UNDEFINE)
			{
				return (m_transportState > UNDEFINE);
			}
			zmq_poll(0, 0, 10);
		}
	}

	void Stop()
	{
		m_bRun = false;
		m_transportState = UNDEFINE;
		DeleteFrontSockets();
		if (m_recvthread)
		{
			if (m_recvthread->joinable())
			{
				if (boost::this_thread::get_id() != m_recvPollThreadid)
				{
					if (m_recvthread)
					{
						m_recvthread->join();
					}
				}
				else
				{
					GLINFO << "warning, zmq stop thread is same poll thread";
				}
			}
		}
		m_identity = std::string();
	}

	bool SetMonitorSocket(zmqcpp::Monitor &monitor, zmqcpp::Socket &sock)
	{
		monitor.SetOnEventConnected(boost::bind(&ZmqClientBase::OnEventConnected, this, _1, _2));
		monitor.SetOnEventConnectDelayed(boost::bind(&ZmqClientBase::OnEventConnectDelayed, this, _1, _2));
		monitor.SetOnEventConnectRetried(boost::bind(&ZmqClientBase::OnEventConnectRetried, this, _1, _2));
		monitor.SetOnEventClosed(boost::bind(&ZmqClientBase::OnEventClosed, this, _1, _2));
		monitor.SetOnEventCloseFailed(boost::bind(&ZmqClientBase::OnEventCloseFailed, this, _1, _2));
		monitor.SetOnEventDisconnected(boost::bind(&ZmqClientBase::OnEventDisconnected, this, _1, _2));
		monitor.SetOnEventUnknown(boost::bind(&ZmqClientBase::OnEventUnknown, this, _1, _2));
		monitor.SetOnEventStop(boost::bind(&ZmqClientBase::OnEventStop, this, _1, _2));

		std::string monitorstr = "inproc://monitorstr#" + m_identity;
		return monitor.SetSocket(sock, m_context, monitorstr, ZMQ_EVENT_ALL);
	}

	void ClientSocketRecv(zmqcpp::Socket &sock)
	{
		UpdateLastPong();

		iovec vec[IOVET_SIZE] = { 0 };
		size_t count = IOVET_SIZE;
		if (sock.Recviov(vec, &count, 0))
		{
			//to do callback
			if (m_recvFunc)
			{
				m_recvFunc(vec, count);
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

	bool SetClientSocket(zmqcpp::Socket &sock)
	{
		if (!sock.SetSocketOption(ZMQ_LINGER, 0))
		{
			GLERROR << "error, SetClientSocket ZMQ_LINGER" << ", code=" << zmq_strerror(zmq_errno());
			return NULL;
		}

		if (!sock.SetSocketOption(ZMQ_SNDTIMEO, 1000))
		{
			GLERROR << "error, SetClientSocket ZMQ_SNDTIMEO" << ", code=" << zmq_strerror(zmq_errno());
			return false;
		}
		if (!sock.SetSocketOption(ZMQ_RCVTIMEO, 1000))
		{
			GLERROR << "error, SetClientSocket ZMQ_RCVTIMEO" << ", code=" << zmq_strerror(zmq_errno());
			return false;
		}
		if (!sock.SetSocketOption(ZMQ_IMMEDIATE, 1))
		{
			GLERROR << "error, SetClientSocket ZMQ_IMMEDIATE" << ", code=" << zmq_strerror(zmq_errno());
			return false;
		}
		if (!sock.SetSocketOption(ZMQ_RECONNECT_IVL, -1))
		{
			GLERROR << "error, SetClientSocket ZMQ_RECONNECT_IVL" << ", code=" << zmq_strerror(zmq_errno());
			return false;
		}
		if (!sock.SetSocketOption(ZMQ_RCVHWM, 0))
		{
			GLERROR << "error, SetClientSocket ZMQ_RCVHWM" << ", code=" << zmq_strerror(zmq_errno());
			return false;
		}
		if (!sock.SetSocketOption(ZMQ_SNDHWM, 0))
		{
			GLERROR << "error, SetClientSocket ZMQ_SNDHWM" << ", code=" << zmq_strerror(zmq_errno());
			return false;
		}
		// 开启keepalive属性
		if (!sock.SetSocketOption(ZMQ_TCP_KEEPALIVE, 1))
		{
			GLERROR << "error, SetClientSocket ZMQ_TCP_KEEPALIVE" << ", code=" << zmq_strerror(zmq_errno());
			return false;
		}
		if (m_heartbeatInterval != 0)
		{
			// 如该连接在心跳内没有任何数据往来,则进行探测 
			if (!sock.SetSocketOption(ZMQ_TCP_KEEPALIVE_IDLE, m_heartbeatInterval/1000))
			{
				GLERROR << "error, SetClientSocket ZMQ_TCP_KEEPALIVE_IDLE" << ", code=" << zmq_strerror(zmq_errno());
				return false;
			}
		}
		else
		{
			if (!sock.SetSocketOption(ZMQ_TCP_KEEPALIVE_IDLE, 10))
			{
				GLERROR << "error, SetClientSocket ZMQ_TCP_KEEPALIVE_IDLE" << ", code=" << zmq_strerror(zmq_errno());
				return false;
			}
		}
		// 探测时发包的时间间隔为2秒
		if (!sock.SetSocketOption(ZMQ_TCP_KEEPALIVE_INTVL, 2))
		{
			GLERROR << "error, SetClientSocket ZMQ_TCP_KEEPALIVE_INTVL" << ", code=" << zmq_strerror(zmq_errno());
			return false;
		}
		// 探测尝试的次数.如果第1次探测包就收到响应了,则后2次的不再发.
		if (!sock.SetSocketOption(ZMQ_TCP_KEEPALIVE_CNT, 3))
		{
			GLERROR << "error, SetClientSocket ZMQ_TCP_KEEPALIVE_CNT" << ", code=" << zmq_strerror(zmq_errno());
			return false;
		}
		if (!sock.SetSocketOption(ZMQ_IDENTITY, m_identity.c_str(), m_identity.size()))
		{
			GLERROR << "error, SetClientSocket ZMQ_IDENTITY" << ", code=" << zmq_strerror(zmq_errno());
			return false;
		}
		if (!sock.Connect(m_address))
		{
			GLERROR << "error, " << "client connect failed, address=" << m_address;
			return false;
		}

		GLINFO << "client connect address=" << m_address;
		return true;
	}



	bool SetBackSocket(zmqcpp::Socket &sock)
	{
		if (!sock.SetSocketOption(ZMQ_LINGER, 0))
		{
			GLERROR << "error, SetBackSocket ZMQ_LINGER" << ", code=" << zmq_strerror(zmq_errno());
			return false;
		}
		if (!sock.SetSocketOption(ZMQ_SNDTIMEO, 1000))
		{
			GLERROR << "error, SetBackSocket ZMQ_SNDTIMEO" << ", code=" << zmq_strerror(zmq_errno());
			return false;
		}
		if (!sock.SetSocketOption(ZMQ_RCVTIMEO, 1000))
		{
			GLERROR << "error, SetBackSocket ZMQ_RCVTIMEO" << ", code=" << zmq_strerror(zmq_errno());
			return false;
		}
		if (!sock.SetSocketOption(ZMQ_RCVHWM, 1000))
		{
			GLERROR << "error, SetBackSocket ZMQ_CONFLATE" << ", code=" << zmq_strerror(zmq_errno());
			return NULL;
		}
		m_inprocAddress = "inproc://back-front&socket#" + m_identity;
		if (!sock.Bind(m_inprocAddress))
		{
			GLERROR << "error, back socket bind address=" << m_inprocAddress << ", code=" << zmq_strerror(zmq_errno());
			return false;
		}
		GLINFO << "back socket bind address=" << m_inprocAddress;
		return true;
	}

	boost::shared_ptr<zmqcpp::Socket> NewFrontSockets()
	{
		boost::mutex::scoped_lock locker(m_frontMutex);
		boost::shared_ptr<zmqcpp::Socket> sock = m_frontsockets[boost::this_thread::get_id()];
		if (!sock)
		{
			sock.reset(new zmqcpp::Socket(m_context, zmqcpp::Socket::PUSH));
			if (!sock->NewSocket())
			{
				GLERROR << "error, NewFrontSockets NewSocket" << ", code=" << zmq_strerror(zmq_errno());
				return NULL;
			}
			if (!sock->SetSocketOption(ZMQ_LINGER, 0))
			{
				GLERROR << "error, NewFrontSockets ZMQ_LINGER" << ", code=" << zmq_strerror(zmq_errno());
				return NULL;
			}
			if (!sock->SetSocketOption(ZMQ_SNDTIMEO, 1000))
			{
				GLERROR << "error, NewFrontSockets ZMQ_SNDTIMEO" << ", code=" << zmq_strerror(zmq_errno());
				return NULL;
			}
			if (!sock->SetSocketOption(ZMQ_RCVTIMEO, 1000))
			{
				GLERROR << "error, NewFrontSockets ZMQ_RCVTIMEO" << ", code=" << zmq_strerror(zmq_errno());
				return NULL;
			}
			if (!sock->SetSocketOption(ZMQ_SNDHWM, 1000))
			{
				GLERROR << "error, NewFrontSockets ZMQ_CONFLATE" << ", code=" << zmq_strerror(zmq_errno());
				return NULL;
			}
			if (!sock->Connect(m_inprocAddress))
			{
				GLERROR << "error, NewFrontSockets connect failed address=" << m_inprocAddress << ", code=" << zmq_strerror(zmq_errno());
				return NULL;
			}
			m_frontsockets[boost::this_thread::get_id()] = sock;
			GLINFO << "front sockets connect, address=" << m_inprocAddress;
		}
		return sock;
	}

	void DeleteFrontSockets()
	{
		boost::mutex::scoped_lock locker(m_frontMutex);
		std::map< boost::thread::id, boost::shared_ptr<zmqcpp::Socket> >::iterator iter;
		for (iter = m_frontsockets.begin(); iter != m_frontsockets.end();)
		{
			iter->second->Disconnect(m_inprocAddress);
			m_frontsockets.erase(iter++);
			zmq_poll(0, 0, POLL_MS);
		}
	}

	void OnEventConnected(const zmq_event_t &event, const char*addr)
	{
		m_transportState = TRANSPORTED;
		if (m_connectStateFunc)
		{
			m_connectStateFunc(ON_EVENT_CONNECTED);
		}
	}
	void OnEventConnectDelayed(const zmq_event_t &event, const char*addr)
	{
		if (m_connectStateFunc)
		{
			m_connectStateFunc(ON_EVENT_CONNECT_DELAYED);
		}
	}
	void OnEventConnectRetried(const zmq_event_t &event, const char*addr)
	{
		if (m_connectStateFunc)
		{
			m_connectStateFunc(ON_EVENT_CONNECT_RETRIED);
		}
	}
	void OnEventClosed(const zmq_event_t &event, const char*addr)
	{
		m_transportState = TRANSPORT_FAILED;
		m_bRun = false;
		if (m_connectStateFunc)
		{
			m_connectStateFunc(ON_EVENT_CLOSED);
		}
	}
	void OnEventCloseFailed(const zmq_event_t &event, const char*addr)
	{
		if (m_connectStateFunc)
		{
			m_connectStateFunc(ON_EVENT_CLOSE_FAILED);
		}
	}
	void OnEventDisconnected(const zmq_event_t &event, const char*addr)
	{
		m_transportState = TRANSPORT_FAILED;
		m_bRun = false;
		if (m_connectStateFunc)
		{
			m_connectStateFunc(ON_EVENT_DISCONNECTED);
		}
	}
	void OnEventUnknown(const zmq_event_t &event, const char*addr)
	{
		if (m_connectStateFunc)
		{
			m_connectStateFunc(ON_EVENT_UNKNOWN);
		}
	}

	void OnEventStop(const zmq_event_t &event, const char*addr)
	{
		m_bStopMonitor = true;
	}

	void UpdateLastPong()
	{
		m_lastpong = zmqcpp::GetTick();
	}

	bool				m_bAutoConnect;
	uint32_t			m_heartbeatInterval;
	uint32_t			m_heartbeatDelay;
	zmqcpp::Tick		m_lastpong;
	ConnectStateFunc	m_connectStateFunc;
	RecvFunc			m_recvFunc;

	std::string m_address;
	std::string m_inprocAddress;

	zmqcpp::Context m_context;

	std::string m_identity;
	boost::thread::id m_recvPollThreadid;
	boost::atomic_bool m_bRun;
	boost::atomic<EnumTransportState> m_transportState;
	boost::shared_ptr< boost::thread > m_recvthread;
	boost::mutex m_frontMutex;
	std::map< boost::thread::id, boost::shared_ptr<zmqcpp::Socket> > m_frontsockets;
	EASI_SOCKET_NAMESPACE::TimerWorker             m_autoConnectTimer;
	EnumZmqClientType       m_clientType;
	bool					m_bStopMonitor;
};

#endif // __8DE54530D7F4459CAAD8F3994AA20712_H_
