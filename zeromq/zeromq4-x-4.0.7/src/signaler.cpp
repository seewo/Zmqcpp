/*
    Copyright (c) 2007-2013 Contributors as noted in the AUTHORS file

    This file is part of 0MQ.

    0MQ is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    0MQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "platform.hpp"

#if defined ZMQ_FORCE_SELECT
#define ZMQ_SIGNALER_WAIT_BASED_ON_SELECT
#elif defined ZMQ_FORCE_POLL
#define ZMQ_SIGNALER_WAIT_BASED_ON_POLL
#elif defined ZMQ_HAVE_LINUX || defined ZMQ_HAVE_FREEBSD ||\
    defined ZMQ_HAVE_OPENBSD || defined ZMQ_HAVE_SOLARIS ||\
    defined ZMQ_HAVE_OSX || defined ZMQ_HAVE_QNXNTO ||\
    defined ZMQ_HAVE_HPUX || defined ZMQ_HAVE_AIX ||\
    defined ZMQ_HAVE_NETBSD
#define ZMQ_SIGNALER_WAIT_BASED_ON_POLL
#elif defined ZMQ_HAVE_WINDOWS || defined ZMQ_HAVE_OPENVMS ||\
	defined ZMQ_HAVE_CYGWIN
#define ZMQ_SIGNALER_WAIT_BASED_ON_SELECT
#endif

//  On AIX, poll.h has to be included before zmq.h to get consistent
//  definition of pollfd structure (AIX uses 'reqevents' and 'retnevents'
//  instead of 'events' and 'revents' and defines macros to map from POSIX-y
//  names to AIX-specific names).
#if defined ZMQ_SIGNALER_WAIT_BASED_ON_POLL
#include <poll.h>
#elif defined ZMQ_SIGNALER_WAIT_BASED_ON_SELECT
#if defined ZMQ_HAVE_WINDOWS
#include "windows.hpp"
#elif defined ZMQ_HAVE_HPUX
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#elif defined ZMQ_HAVE_OPENVMS
#include <sys/types.h>
#include <sys/time.h>
#else
#include <sys/select.h>
#endif
#endif

#include "signaler.hpp"
#include "likely.hpp"
#include "stdint.hpp"
#include "config.hpp"
#include "err.hpp"
#include "fd.hpp"
#include "ip.hpp"

#if defined ZMQ_HAVE_EVENTFD
#include <sys/eventfd.h>
#endif

#if defined ZMQ_HAVE_WINDOWS
#include "windows.hpp"
#else
#include <unistd.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include<stdio.h>
#include<stdlib.h>
#include <sstream>
#include <iostream>
#include "random.hpp"
#include "clock.hpp"

#ifdef ZMQ_HAVE_WINDOWS
#include <Iphlpapi.h>
#pragma comment(lib,"Iphlpapi.lib")

static std::string s_rand_socketpair_scope_event_string;

static void gen_rand_socketpair_scope_event_string()
{
	if (s_rand_socketpair_scope_event_string.empty())
	{
		zmq::seed_random();
		uint32_t randNumber = zmq::generate_random();
		std::stringstream ss;
		ss << randNumber << zmq::clock_t::now_us();
		s_rand_socketpair_scope_event_string = ss.str();
	}
}

// 事件封装在类里面，防止在崩溃时不退出全局事件。
class scope_event_t
{
public:
	scope_event_t()
		:m_sync(NULL)
	{
		start();
	}

	~scope_event_t()
	{
		stop();
	}

	void wait()
	{
		if (m_sync)
		{
			//  Enter the critical section.
			DWORD dwrc = WaitForSingleObject(m_sync, INFINITE);
			zmq_assert(dwrc == WAIT_OBJECT_0);
		}
	}

	void unwait()
	{
		stop();
	}

private:
	void start()
	{
#if !defined _WIN32_WCE
		SECURITY_DESCRIPTOR sd;
		SECURITY_ATTRIBUTES sa;
		memset(&sd, 0, sizeof (sd));
		memset(&sa, 0, sizeof (sa));

		InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
		SetSecurityDescriptorDacl(&sd, TRUE, 0, FALSE);

		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.lpSecurityDescriptor = &sd;
#endif
		std::string lpname = TEXT("Global\\zmq-signaler-port-sync-seewo-fwq") + s_rand_socketpair_scope_event_string;
#if !defined _WIN32_WCE
		m_sync = CreateEventA(&sa, FALSE, TRUE, lpname.c_str());
#else
		m_sync = CreateEventA(NULL, FALSE, TRUE, lpname.c_str());
#endif
		if (m_sync == NULL && GetLastError() == ERROR_ACCESS_DENIED)
			m_sync = OpenEventA(SYNCHRONIZE | EVENT_MODIFY_STATE,
			FALSE, lpname.c_str());
		win_assert(m_sync != NULL);
	}

	void stop()
	{
		if (m_sync)
		{
			//  Exit the critical section.
			BOOL brc = SetEvent(m_sync);
			win_assert(brc != 0);

			//  Release the kernel object
			brc = CloseHandle(m_sync);
			win_assert(brc != 0);
			m_sync = NULL;
		}
	}
	HANDLE m_sync;
};
#endif

#if !defined (ZMQ_HAVE_WINDOWS)
// Helper to sleep for specific number of milliseconds (or until signal)
//
static int sleep_ms (unsigned int ms_)
{
    if (ms_ == 0)
        return 0;
#if defined ZMQ_HAVE_WINDOWS
    Sleep (ms_ > 0 ? ms_ : INFINITE);
    return 0;
#elif defined ZMQ_HAVE_ANDROID
    usleep (ms_ * 1000);
    return 0;
#else
    return usleep (ms_ * 1000);
#endif
}

// Helper to wait on close(), for non-blocking sockets, until it completes
// If EAGAIN is received, will sleep briefly (1-100ms) then try again, until
// the overall timeout is reached.
//
static int close_wait_ms (int fd_, unsigned int max_ms_ = 2000)
{
    unsigned int ms_so_far = 0;
    unsigned int step_ms   = max_ms_ / 10;
    if (step_ms < 1)
        step_ms = 1;

    if (step_ms > 100)
        step_ms = 100;

    int rc = 0;       // do not sleep on first attempt

    do
    {
        if (rc == -1 && errno == EAGAIN)
        {
            sleep_ms (step_ms);
            ms_so_far += step_ms;
        }

        rc = close (fd_);
    } while (ms_so_far < max_ms_ && rc == -1 && errno == EAGAIN);

    return rc;
}
#endif

zmq::signaler_t::signaler_t ()
{
#ifdef ZMQ_HAVE_WINDOWS
	gen_rand_socketpair_scope_event_string();
#endif
    //  Create the socketpair for signaling.
    if (make_fdpair (&r, &w) == 0) {
        unblock_socket (w);
        unblock_socket (r);
    }
#ifdef HAVE_FORK
    pid = getpid();
#endif
}

zmq::signaler_t::~signaler_t ()
{
#if defined ZMQ_HAVE_EVENTFD
    int rc = close_wait_ms (r);
    errno_assert (rc == 0);
#elif defined ZMQ_HAVE_WINDOWS
    struct linger so_linger = { 1, 0 };
    int rc = setsockopt (w, SOL_SOCKET, SO_LINGER,
        (const char *) &so_linger, sizeof so_linger);
    //  Only check shutdown if WSASTARTUP was previously done
    if (rc == 0 || WSAGetLastError () != WSANOTINITIALISED) {
        wsa_assert (rc != SOCKET_ERROR);
        rc = closesocket (w);
        wsa_assert (rc != SOCKET_ERROR);
        rc = closesocket (r);
        wsa_assert (rc != SOCKET_ERROR);
    }
#else
    int rc = close_wait_ms (w);
    errno_assert (rc == 0);
    rc = close_wait_ms (r);
    errno_assert (rc == 0);
#endif
}

zmq::fd_t zmq::signaler_t::get_fd ()
{
    return r;
}

void zmq::signaler_t::send ()
{
#if HAVE_FORK
    if (unlikely(pid != getpid())) {
        //printf("Child process %d signaler_t::send returning without sending #1\n", getpid());
        return; // do not send anything in forked child context
    }
#endif
#if defined ZMQ_HAVE_EVENTFD
    const uint64_t inc = 1;
    ssize_t sz = write (w, &inc, sizeof (inc));
    errno_assert (sz == sizeof (inc));
#elif defined ZMQ_HAVE_WINDOWS
    unsigned char dummy = 0;
    int nbytes = ::send (w, (char*) &dummy, sizeof (dummy), 0);
    wsa_assert (nbytes != SOCKET_ERROR);
    zmq_assert (nbytes == sizeof (dummy));
#else
    unsigned char dummy = 0;
    while (true) {
        ssize_t nbytes = ::send (w, &dummy, sizeof (dummy), 0);
        if (unlikely (nbytes == -1 && errno == EINTR))
            continue;
#if HAVE_FORK
        if (unlikely(pid != getpid())) {
            //printf("Child process %d signaler_t::send returning without sending #2\n", getpid());
            errno = EINTR;
            break;
        }
#endif
        zmq_assert (nbytes == sizeof (dummy));
        break;
    }
#endif
}

int zmq::signaler_t::wait (int timeout_)
{
#ifdef HAVE_FORK
    if (unlikely(pid != getpid()))
    {
        // we have forked and the file descriptor is closed. Emulate an interupt
        // response.
        //printf("Child process %d signaler_t::wait returning simulating interrupt #1\n", getpid());
        errno = EINTR;
        return -1;
    }
#endif

#ifdef ZMQ_SIGNALER_WAIT_BASED_ON_POLL

    struct pollfd pfd;
    pfd.fd = r;
    pfd.events = POLLIN;
    int rc = poll (&pfd, 1, timeout_);
    if (unlikely (rc < 0)) {
        errno_assert (errno == EINTR);
        return -1;
    }
    else
    if (unlikely (rc == 0)) {
        errno = EAGAIN;
        return -1;
    }
#ifdef HAVE_FORK
    if (unlikely(pid != getpid())) {
        // we have forked and the file descriptor is closed. Emulate an interupt
        // response.
        //printf("Child process %d signaler_t::wait returning simulating interrupt #2\n", getpid());
        errno = EINTR;
        return -1;
    }
#endif
    zmq_assert (rc == 1);
    zmq_assert (pfd.revents & POLLIN);
    return 0;

#elif defined ZMQ_SIGNALER_WAIT_BASED_ON_SELECT

    fd_set fds;
    FD_ZERO (&fds);
    FD_SET (r, &fds);
    struct timeval timeout;
    if (timeout_ >= 0) {
        timeout.tv_sec = timeout_ / 1000;
        timeout.tv_usec = timeout_ % 1000 * 1000;
    }
#ifdef ZMQ_HAVE_WINDOWS
    int rc = select (0, &fds, NULL, NULL,
        timeout_ >= 0 ? &timeout : NULL);
    wsa_assert (rc != SOCKET_ERROR);
#else
    int rc = select (r + 1, &fds, NULL, NULL,
        timeout_ >= 0 ? &timeout : NULL);
    if (unlikely (rc < 0)) {
        errno_assert (errno == EINTR);
        return -1;
    }
#endif
    if (unlikely (rc == 0)) {
        errno = EAGAIN;
        return -1;
    }
    zmq_assert (rc == 1);
    return 0;

#else
#error
#endif
}

void zmq::signaler_t::recv ()
{
    //  Attempt to read a signal.
#if defined ZMQ_HAVE_EVENTFD
    uint64_t dummy;
    ssize_t sz = read (r, &dummy, sizeof (dummy));
    errno_assert (sz == sizeof (dummy));

    //  If we accidentally grabbed the next signal along with the current
    //  one, return it back to the eventfd object.
    if (unlikely (dummy == 2)) {
        const uint64_t inc = 1;
        ssize_t sz2 = write (w, &inc, sizeof (inc));
        errno_assert (sz2 == sizeof (inc));
        return;
    }

    zmq_assert (dummy == 1);
#else
    unsigned char dummy;
#if defined ZMQ_HAVE_WINDOWS
    int nbytes = ::recv (r, (char*) &dummy, sizeof (dummy), 0);
    wsa_assert (nbytes != SOCKET_ERROR);
#else
    ssize_t nbytes = ::recv (r, &dummy, sizeof (dummy), 0);
    errno_assert (nbytes >= 0);
#endif
    zmq_assert (nbytes == sizeof (dummy));
    zmq_assert (dummy == 0);
#endif
}

#ifdef HAVE_FORK
void zmq::signaler_t::forked()
{
    //  Close file descriptors created in the parent and create new pair
    close (r);
    close (w);
    make_fdpair (&r, &w);
}
#endif

bool zmq::signaler_t::generate_socketpair(fd_t *rfd, fd_t *wfd)
{
#ifdef ZMQ_HAVE_WINDOWS
	scope_event_t se;

	fd_t read_fd = INVALID_SOCKET;
	fd_t write_fd = INVALID_SOCKET;

	//  Create listening socket.
	SOCKET listen_fd = open_socket(AF_INET, SOCK_STREAM, 0);
	wsa_assert(listen_fd != INVALID_SOCKET);

	BOOL tcp_nodelay = 1;
	int rc = setsockopt(listen_fd, IPPROTO_TCP, TCP_NODELAY,
		(char *)&tcp_nodelay, sizeof (tcp_nodelay));
	wsa_assert(rc != SOCKET_ERROR);

	//  Init sockaddr to signaler port.
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof (addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(0);
	//  Create the writer socket.
	write_fd = open_socket(AF_INET, SOCK_STREAM, 0);
	wsa_assert(write_fd != INVALID_SOCKET);

	//  Set TCP_NODELAY on writer socket.
	rc = setsockopt(write_fd, IPPROTO_TCP, TCP_NODELAY,
		(char *)&tcp_nodelay, sizeof (tcp_nodelay));
	wsa_assert(rc != SOCKET_ERROR);

	//  Enter the critical section.
	se.wait();

	//  Bind listening socket to signaler port.
	rc = bind(listen_fd, (const struct sockaddr*) &addr, sizeof (addr));
	if (rc != 0)
	{
		std::cout << "zmq bind failed" << std::endl;
		goto failed_go_to;
	}

	rc = listen(listen_fd, 1);
	if (rc != 0)
	{
		std::cout << "zmq listen failed" << std::endl;
		goto failed_go_to;
	}

	struct sockaddr_in addrtemp;
	int lenaddr = sizeof(addrtemp);
	memset(&addr, 0, sizeof (addrtemp));
	if (getsockname(listen_fd, (struct sockaddr*) &addrtemp, &lenaddr) != 0)
	{
		std::cout << "zmq getsockname failed" << std::endl;
		goto failed_go_to;
	}

	int port = ntohs(addrtemp.sin_port);
	rc = connect(write_fd, (struct sockaddr*) &addrtemp, sizeof (addrtemp));
	if (rc != 0)
	{
		std::cout << "zmq connect failed" << ", port=" << port << std::endl;
		goto failed_go_to;
	}

	read_fd = accept(listen_fd, NULL, NULL);
	//  Save errno if error occurred in bind/listen/connect/accept.
	if (read_fd == INVALID_SOCKET)
	{
		goto failed_go_to;
	}

	if (listen_fd != INVALID_SOCKET)
	{
		closesocket(listen_fd);
	}

	*rfd = read_fd;
	*wfd = write_fd;

	std::cout << "zmq socketpair" << ", port=" << port << std::endl;
	return true;

failed_go_to:
	if (listen_fd != INVALID_SOCKET)
	{
		closesocket(listen_fd);
	}

	if (write_fd != INVALID_SOCKET)
	{
		struct linger so_linger = { 1, 0 };
		int rc = setsockopt(write_fd, SOL_SOCKET, SO_LINGER, (const char *)&so_linger, sizeof so_linger);
		//  Only check shutdown if WSASTARTUP was previously done
		if (rc == 0 || WSAGetLastError() != WSANOTINITIALISED)
		{
			wsa_assert(rc != SOCKET_ERROR);
			rc = closesocket(write_fd);
			wsa_assert(rc != SOCKET_ERROR);
			if (read_fd != INVALID_SOCKET)
			{
				rc = closesocket(read_fd);
			}
			wsa_assert(rc != SOCKET_ERROR);
		}
	}
	return false;
#endif
	return true;
}

//  Returns -1 if we could not make the socket pair successfully
int zmq::signaler_t::make_fdpair (fd_t *r_, fd_t *w_)
{
#if defined ZMQ_HAVE_EVENTFD
    fd_t fd = eventfd (0, 0);
    if (fd == -1) {
        errno_assert (errno == ENFILE || errno == EMFILE);
        *w_ = *r_ = -1;
        return -1;
    }
    else {
        *w_ = *r_ = fd;
        return 0;
    }

#elif defined ZMQ_HAVE_WINDOWS
    //  Windows has no 'socketpair' function. CreatePipe is no good as pipe
    //  handles cannot be polled on. Here we create the socketpair by hand.
    *w_ = INVALID_SOCKET;
    *r_ = INVALID_SOCKET;

    //  Enter the critical section.
	int saved_errno = 0;
	BOOL brc = FALSE;
	while (true)
	{
		BOOL brc = generate_socketpair(r_, w_);
		if (brc)
		{
			break;
		}
		else
		{
			Sleep(10);
		}
	}

    if (*r_ != INVALID_SOCKET) {
#   if !defined _WIN32_WCE
        //  On Windows, preventing sockets to be inherited by child processes.
		BOOL brc = SetHandleInformation((HANDLE)*r_, HANDLE_FLAG_INHERIT, 0);
        win_assert (brc);
#   endif
        return 0;
    }
    else {
        //  Cleanup writer if connection failed
        if (*w_ != INVALID_SOCKET) {
            int rc = closesocket (*w_);
            wsa_assert (rc != SOCKET_ERROR);
            *w_ = INVALID_SOCKET;
        }
        //  Set errno from saved value
        errno = wsa_error_to_errno (saved_errno);
        return -1;
    }

#elif defined ZMQ_HAVE_OPENVMS

    //  Whilst OpenVMS supports socketpair - it maps to AF_INET only.  Further,
    //  it does not set the socket options TCP_NODELAY and TCP_NODELACK which
    //  can lead to performance problems.
    //
    //  The bug will be fixed in V5.6 ECO4 and beyond.  In the meantime, we'll
    //  create the socket pair manually.
    struct sockaddr_in lcladdr;
    memset (&lcladdr, 0, sizeof (lcladdr));
    lcladdr.sin_family = AF_INET;
    lcladdr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    lcladdr.sin_port = 0;

    int listener = open_socket (AF_INET, SOCK_STREAM, 0);
    errno_assert (listener != -1);

    int on = 1;
    int rc = setsockopt (listener, IPPROTO_TCP, TCP_NODELAY, &on, sizeof (on));
    errno_assert (rc != -1);

    rc = setsockopt (listener, IPPROTO_TCP, TCP_NODELACK, &on, sizeof (on));
    errno_assert (rc != -1);

    rc = bind (listener, (struct sockaddr*) &lcladdr, sizeof (lcladdr));
    errno_assert (rc != -1);

    socklen_t lcladdr_len = sizeof (lcladdr);

    rc = getsockname (listener, (struct sockaddr*) &lcladdr, &lcladdr_len);
    errno_assert (rc != -1);

    rc = listen (listener, 1);
    errno_assert (rc != -1);

    *w_ = open_socket (AF_INET, SOCK_STREAM, 0);
    errno_assert (*w_ != -1);

    rc = setsockopt (*w_, IPPROTO_TCP, TCP_NODELAY, &on, sizeof (on));
    errno_assert (rc != -1);

    rc = setsockopt (*w_, IPPROTO_TCP, TCP_NODELACK, &on, sizeof (on));
    errno_assert (rc != -1);

    rc = connect (*w_, (struct sockaddr*) &lcladdr, sizeof (lcladdr));
    errno_assert (rc != -1);

    *r_ = accept (listener, NULL, NULL);
    errno_assert (*r_ != -1);

    close (listener);

    return 0;

#else
    // All other implementations support socketpair()
    int sv [2];
    int rc = socketpair (AF_UNIX, SOCK_STREAM, 0, sv);
    if (rc == -1) {
        errno_assert (errno == ENFILE || errno == EMFILE);
        *w_ = *r_ = -1;
        return -1;
    }
    else {
        *w_ = sv [0];
        *r_ = sv [1];
        return 0;
    }
#endif
}

#if defined ZMQ_SIGNALER_WAIT_BASED_ON_SELECT
#undef ZMQ_SIGNALER_WAIT_BASED_ON_SELECT
#endif
#if defined ZMQ_SIGNALER_WAIT_BASED_ON_POLL
#undef ZMQ_SIGNALER_WAIT_BASED_ON_POLL
#endif

