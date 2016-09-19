/*******************************************************************
*  Copyright(c) 2000-2015 Guangzhou Shiyuan Electronics Co., Ltd.
*  All rights reserved.
*
*  FileName:		ThreadGroup
*  Author:			libin
*  Date:			2015/12/20
*  Description:
******************************************************************/

#ifndef __AC6F8D75E4BF4FEEBB543015F188B19C_H__
#define __AC6F8D75E4BF4FEEBB543015F188B19C_H__

#include <vector>
#include <string>

#include <boost/atomic.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio/strand.hpp>
#include "SocketCommon.h"

EASI_SOCKET_NAMESPACE_BEGEIN

//����ASIO��BIND FUNCTION��ʵ���̳߳�
class ThreadGroupImpl : public boost::noncopyable
{
public:

	ThreadGroupImpl(int threadNumber = 1, const std::string &name = std::string());
	~ThreadGroupImpl();

	bool Start();

	void Stop();

	boost::asio::io_service &IOService() { return m_ioService; }

	int ThreadNumbers() const { return m_initNumbers; }

	template< typename CompletionHandler >
	void Dispatch(CompletionHandler handler)
	{
		m_ioService.dispatch(handler);
	}

	template< typename CompletionHandler >
	void Post(CompletionHandler handler)
	{
		m_ioService.post(handler);
	}

	// post����ʲô�����������񶪵������У�Ȼ���������ء�
	void Post(boost::function< void(void) > handler)
	{
		m_ioService.post(handler);
	}

	// dispatch�����run()��һ���̣߳���ô�����ֱ����dispatch�ڲ����ã�ִ�н����󷵻ء�����һ���̸߳�postһ����
	// ��ǰʵ�ֲ���ͬһ�߳��У��������ʹ��Post������
	void Dispatch(boost::function< void(void) > handle)
	{
		m_ioService.dispatch(handle);
	}

	// ���ز�����
	template<typename F, typename A>
	void Post(F f, A a) { Post(boost::bind(f, a)); }
	template<typename F, typename A, typename B>
	void Post(F f, A a, B b) { Post(boost::bind(f, a, b)); }
	template<typename F, typename A, typename B, typename C>
	void Post(F f, A a, B b, C c) { Post(boost::bind(f, a, b, c)); }
	template<typename F, typename A, typename B, typename C, typename D>
	void Post(F f, A a, B b, C c, D d) { Post(boost::bind(f, a, b, c, d)); }
	template<typename F, typename A, typename B, typename C, typename D, typename E>
	void Post(F f, A a, B b, C c, D d, E e) { Post(boost::bind(f, a, b, c, d, e)); }

	// ���ز�����
	template<typename F, typename A>
	void Dispatch(F f, A a) { Dispatch(boost::bind(f, a)); }
	template<typename F, typename A, typename B>
	void Dispatch(F f, A a, B b) { Dispatch(boost::bind(f, a, b)); }
	template<typename F, typename A, typename B, typename C>
	void Dispatch(F f, A a, B b, C c) { Dispatch(boost::bind(f, a, b, c)); }
	template<typename F, typename A, typename B, typename C, typename D>
	void Dispatch(F f, A a, B b, C c, D d) { Dispatch(boost::bind(f, a, b, c, d)); }
	template<typename F, typename A, typename B, typename C, typename D, typename E>
	void Dispatch(F f, A a, B b, C c, D d, E e) { Dispatch(boost::bind(f, a, b, c, d, e)); }

private:
	void ThreadsRun();

	volatile bool m_bRunning;
	int m_threadNumbers;
	std::string m_name;
	boost::asio::io_service m_ioService;
	boost::asio::io_service::strand m_strand;
	boost::shared_ptr< boost::asio::io_service::work > m_spIoServiceWork;
	std::vector< boost::shared_ptr< boost::thread > > m_threads;
	boost::atomic<int> m_initNumbers;
};

EASI_SOCKET_NAMESPACE_END

#endif // __AC6F8D75E4BF4FEEBB543015F188B19C_H_
