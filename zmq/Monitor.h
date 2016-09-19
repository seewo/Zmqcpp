/*******************************************************************
*  Copyright(c) 2000-2016 Guangzhou Shirui Electronics Co., Ltd.
*  All rights reserved.
*
*  FileName:		Monitor
*  Author:			libin
*  Date:			2016/01/05
*  Description:	
******************************************************************/

#ifndef __60626E69AC324C6CBC4AE4C1FBE46883_H__
#define __60626E69AC324C6CBC4AE4C1FBE46883_H__

#include <string>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include "Context.h"
#include "Socket.h"

ZMQ_CPP_NAMESPACE_BEGIN

class Monitor : public boost::noncopyable
{
public:
	typedef boost::function< void() >								  OnMonitorStared;
	typedef boost::function< void() >								  OnMonitorFinish;

	typedef boost::function< void(const zmq_event_t &, const char*) > OnEventConnected;
	typedef boost::function< void(const zmq_event_t &, const char*) > OnEventConnectDelayed;
	typedef boost::function< void(const zmq_event_t &, const char*) > OnEventConnectRetried;
	typedef boost::function< void(const zmq_event_t &, const char*) > OnEventListening;
	typedef boost::function< void(const zmq_event_t &, const char*) > OnEventBindFailed;
	typedef boost::function< void(const zmq_event_t &, const char*) > OnEventAccepted;
	typedef boost::function< void(const zmq_event_t &, const char*) > OnEventAccepFailed;
	typedef boost::function< void(const zmq_event_t &, const char*) > OnEventClosed;
	typedef boost::function< void(const zmq_event_t &, const char*) > OnEventCloseFailed;
	typedef boost::function< void(const zmq_event_t &, const char*) > OnEventDisconnected;
	typedef boost::function< void(const zmq_event_t &, const char*) > OnEventUnknown;
	typedef boost::function< void(const zmq_event_t &, const char*) > OnEventStop;

	Monitor() : m_socket(NULL) { }
	~Monitor() { Close(); }

	void SetOnEventConnected(OnEventConnected event) { m_onEventConnected = event; }
	void SetOnEventConnectDelayed(OnEventConnectDelayed event) { m_onEventConnectDelayed = event; }
	void SetOnEventConnectRetried(OnEventConnectRetried event) { m_onEventConnectRetried = event; }
	void SetOnEventListening(OnEventListening event) { m_onEventListening = event; }
	void SetOnEventBindFailed(OnEventBindFailed event) { m_onEventBindFailed = event; }
	void SetOnEventAccepted(OnEventAccepted event) { m_onEventAccepted = event; }
	void SetOnEventAccepFailed(OnEventAccepFailed event) { m_onEventAccepFailed = event; }
	void SetOnEventClosed(OnEventClosed event) { m_onEventClosed = event; }
	void SetOnEventCloseFailed(OnEventCloseFailed event) { m_onEventCloseFailed = event; }
	void SetOnEventDisconnected(OnEventDisconnected event) { m_onEventDisconnected = event; }
	void SetOnEventUnknown(OnEventUnknown event) { m_onEventUnknown = event; }
	void SetOnEventStop(OnEventStop event) { m_onEventStop = event; }

	void *GetSocket()
	{
		return m_socket;
	}

	const void *GetSocket() const
	{
		return m_socket;
	}

	bool SetSocket(Socket &s, Context &ctx, const std::string &addr, int events = ZMQ_EVENT_ALL)
	{
		Close();
		m_addrss = addr;
		int rc = zmq_socket_monitor(s.GetSocket(), addr.c_str(), events);
		if (rc != 0)
		{
			return false;
		}
		m_socket = zmq_socket(ctx.GetContext(), ZMQ_PAIR);
		int linger = 0;
		rc = zmq_setsockopt(m_socket, ZMQ_LINGER, &linger, sizeof(linger));
		if (rc == -1)
		{
			return false;
		}
		int timeout = 500;
		rc = zmq_setsockopt(m_socket, ZMQ_SNDTIMEO, &timeout, sizeof(timeout));
		if (rc == -1)
		{
			return false;
		}
		rc = zmq_setsockopt(m_socket, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
		if (rc == -1)
		{
			return false;
		}
		if (zmq_connect(m_socket, addr.c_str()) == -1)
		{
			return false;
		}
		return true;
	}

	bool MonitorRecv()
	{		
		zmq_msg_t eventMsg;
		zmq_msg_init(&eventMsg);
		int rc = zmq_msg_recv(&eventMsg, m_socket, 0);
		if (rc == -1 && zmq_errno() == ETERM)
		{
			return false;
		}

		const char* data = static_cast<const char*>(zmq_msg_data(&eventMsg));
		zmq_event_t msgEvent;
		memcpy(&msgEvent.event, data, sizeof(uint16_t)); data += sizeof(uint16_t);
		memcpy(&msgEvent.value, data, sizeof(int32_t));
		zmq_event_t* event = &msgEvent;

		zmq_msg_t addrMsg;
		zmq_msg_init(&addrMsg);
		rc = zmq_msg_recv(&addrMsg, m_socket, 0);
		if (rc == -1 && zmq_errno() == ETERM)
		{
			return false;
		}
		const char* str = static_cast<const char*>(zmq_msg_data(&addrMsg));
		std::string address(str, str + zmq_msg_size(&addrMsg));
		zmq_msg_close(&addrMsg);

		if (event->event == ZMQ_EVENT_MONITOR_STOPPED)
		{
			m_onEventStop(*event, address.c_str());
			return false;
		}

		switch (event->event)
		{
		case ZMQ_EVENT_CONNECTED:
			if (m_onEventConnected)
			{
				m_onEventConnected(*event, address.c_str());
			}
			break;
		case ZMQ_EVENT_CONNECT_DELAYED:
			if (m_onEventConnectDelayed)
			{
				m_onEventConnectDelayed(*event, address.c_str());
			}
			break;
		case ZMQ_EVENT_CONNECT_RETRIED:
			if (m_onEventConnectRetried)
			{
				m_onEventConnectRetried(*event, address.c_str());
			}
			break;
		case ZMQ_EVENT_LISTENING:
			if (m_onEventListening)
			{
				m_onEventListening(*event, address.c_str());
			}
			break;
		case ZMQ_EVENT_BIND_FAILED:
			if (m_onEventListening)
			{
				m_onEventListening(*event, address.c_str());
			}
			break;
		case ZMQ_EVENT_ACCEPTED:
			if (m_onEventAccepted)
			{
				m_onEventAccepted(*event, address.c_str());
			}
			break;
		case ZMQ_EVENT_ACCEPT_FAILED:
			if (m_onEventAccepFailed)
			{
				m_onEventAccepFailed(*event, address.c_str());
			}
			break;
		case ZMQ_EVENT_CLOSED:
			if (m_onEventClosed)
			{
				m_onEventClosed(*event, address.c_str());
			}
			break;
		case ZMQ_EVENT_CLOSE_FAILED:
			if (m_onEventCloseFailed)
			{
				m_onEventCloseFailed(*event, address.c_str());
			}
			break;
		case ZMQ_EVENT_DISCONNECTED:
			if (m_onEventDisconnected)
			{
				m_onEventDisconnected(*event, address.c_str());
			}
			break;
		default:
			if (m_onEventUnknown)
			{
				m_onEventUnknown(*event, address.c_str());
			}
			break;
		}
		zmq_msg_close(&eventMsg);
		return true;
	}

private:
	void Close()
	{
		if (m_socket)
		{
			if (!m_addrss.empty())
			{
				zmq_disconnect(m_socket, m_addrss.c_str());
			}
			zmq_close(m_socket);
			m_socket = NULL;
		}
	}

	OnEventConnected m_onEventConnected;
	OnEventConnectDelayed m_onEventConnectDelayed;
	OnEventConnectRetried m_onEventConnectRetried;
	OnEventListening m_onEventListening;
	OnEventBindFailed m_onEventBindFailed;
	OnEventAccepted m_onEventAccepted;
	OnEventAccepFailed m_onEventAccepFailed;
	OnEventClosed m_onEventClosed;
	OnEventCloseFailed m_onEventCloseFailed;
	OnEventDisconnected m_onEventDisconnected;
	OnEventUnknown m_onEventUnknown;
	OnEventStop m_onEventStop;

	std::string m_addrss;
	void *m_socket;
};

ZMQ_CPP_NAMESPACE_END

#endif // __60626E69AC324C6CBC4AE4C1FBE46883_H_
