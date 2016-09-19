#include "TimerWorker.h"
#include <boost/asio/detail/socket_ops.hpp>
#include <boost/asio/detail/select_interrupter.hpp>
#include <boost/asio/detail/fd_set_adapter.hpp>
#include "ThreadGroupImpl.h"

EASI_SOCKET_NAMESPACE_BEGEIN

TimerWorker::TimerWorker(int milliSeconds)
: m_bRunning(false)
, m_milliSeconds(milliSeconds)
, m_workRoutine(NULL)
{
	m_threadGroup.reset(new ThreadGroupImpl(1));
	m_selectInterrupter.reset(new boost::asio::detail::asio_select_interrupter);
}

TimerWorker::~TimerWorker()
{
	Stop();
	m_threadGroup->Stop();
}

void TimerWorker::Start()
{
	if (m_bRunning)
	{
		return;
	}
	m_bRunning = true;
	m_selectInterrupter.get()->reset();
	if (!m_threadGroup->Start())
	{
		return;
	}
	m_threadGroup->Post(boost::bind(&TimerWorker::OnTimeout, this));
}

void TimerWorker::Stop()
{
	m_bRunning = false;
	m_selectInterrupter->interrupt();
}

void TimerWorker::Wait()
{
	timeval tv;
	tv.tv_sec = m_milliSeconds / 1000;
	tv.tv_usec = (m_milliSeconds % 1000) * 1000;

	int nfds = m_selectInterrupter->read_descriptor() + 1;
	boost::asio::detail::fd_set_adapter fdSet;
	// Set up the descriptor sets.
	fdSet.reset();
	fdSet.set(m_selectInterrupter->read_descriptor());
	fd_set *readFdSet = fdSet;
	boost::system::error_code ec;

	boost::asio::detail::socket_ops::select(nfds, readFdSet, 0, 0, &tv, ec);
}

void TimerWorker::OnTimeout()
{
	if (!m_bRunning)
	{
		return;
	}

	Wait();

	if (!m_bRunning)
	{
		return;
	}

	if (m_workRoutine)
	{
		m_workRoutine();
	}

	m_threadGroup->Post(boost::bind(&TimerWorker::OnTimeout, this));
}

EASI_SOCKET_NAMESPACE_END