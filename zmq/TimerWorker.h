/*******************************************************************
*  Copyright(c) 2000-2016 Guangzhou Shirui Electronics Co., Ltd.
*  All rights reserved.
*
*  FileName:		TimerWorker
*  Author:			libin
*  Date:			2016/02/25
*  Description:	
******************************************************************/

#ifndef __C6FA15F64BAD481785A1B9FD889F2FFA_H__
#define __C6FA15F64BAD481785A1B9FD889F2FFA_H__

#include <boost/asio/detail/config.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include "SocketCommon.h"

namespace boost
{
	namespace asio
	{
		namespace detail
		{
			//copy form <boost/asio/detail/select_interrupter.hpp>
#if defined(BOOST_ASIO_WINDOWS) || defined(__CYGWIN__) || defined(__SYMBIAN32__)
			class socket_select_interrupter;
			typedef socket_select_interrupter asio_select_interrupter;
#elif defined(BOOST_ASIO_HAS_EVENTFD)
			class eventfd_select_interrupter;
			typedef eventfd_select_interrupter asio_select_interrupter;
#else
			class pipe_select_interrupter;
			typedef pipe_select_interrupter asio_select_interrupter;
#endif

		}
	}
}

EASI_SOCKET_NAMESPACE_BEGEIN

class ThreadGroupImpl;

//不怎么精确的定时器，以毫秒为单位。
class TimerWorker
{
public:
	typedef boost::function< void() > WorkRoutine;
public:
	TimerWorker(int milliSeconds = 1000);
	~TimerWorker();
	//毫秒
	void SetTimeMilliSeconds(int milliSeconds) { m_milliSeconds = milliSeconds; }
	void SetWorkRoutine(const WorkRoutine &workRoutine)	{ m_workRoutine = workRoutine; }
	bool IsRunning() { return m_bRunning; }
	void Start();
	void Stop();

private:
	void Wait();
	void OnTimeout();

	boost::scoped_ptr< ThreadGroupImpl >							m_threadGroup;
	volatile bool													m_bRunning;
	WorkRoutine														m_workRoutine;
	int																m_milliSeconds;
	boost::scoped_ptr< boost::asio::detail::asio_select_interrupter	>	m_selectInterrupter;
};

EASI_SOCKET_NAMESPACE_END

#endif // __C6FA15F64BAD481785A1B9FD889F2FFA_H_
