#include "ThreadGroupImpl.h"
#include <boost/make_shared.hpp>

EASI_SOCKET_NAMESPACE_BEGEIN

ThreadGroupImpl::ThreadGroupImpl(int threadNumber, const std::string &name /* = std::string()*/)
: m_bRunning(false)
, m_threadNumbers(threadNumber)
, m_name(name)
, m_initNumbers(0)
, m_strand(m_ioService)
{
	Start();
}

ThreadGroupImpl::~ThreadGroupImpl()
{
	Stop();
}

bool ThreadGroupImpl::Start()
{
	if (m_bRunning)
	{
		return true;
	}
	m_bRunning = true;
	m_spIoServiceWork = boost::make_shared< boost::asio::io_service::work >(m_ioService);

	for (int i = 0; i < m_threadNumbers; ++i)
	{
		boost::shared_ptr< boost::thread > spThreadPtr  = 
			boost::make_shared<boost::thread>(boost::bind(&ThreadGroupImpl::ThreadsRun, this));
		if (!spThreadPtr)
		{
			Stop();
			return false;
		}

		m_threads.push_back(spThreadPtr);
	}

	//等待所有线程初始化结束
	while (true)
	{
		if (m_initNumbers == m_threadNumbers)
		{
			break;
		}
	}
	return true;
}

void ThreadGroupImpl::Stop()
{
	if (!m_bRunning)
	{
		return;
	}
	m_bRunning = false;
	m_spIoServiceWork.reset();
	m_ioService.stop();

	for (int i = 0; i < m_threadNumbers; ++i)
	{
		m_threads[i]->join();
		--m_initNumbers;
	}
	while (!m_threads.empty())
	{
		m_threads.pop_back();
	}
}

void ThreadGroupImpl::ThreadsRun()
{
	++m_initNumbers;
	m_ioService.run();
}

EASI_SOCKET_NAMESPACE_END
