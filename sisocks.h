/*  system independent sockets (basically for unix and Win)
 *  Copyright (C) 2000,1 Simon Urbanek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; version 2.1 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
   
   conditional defines: 

   MAIN
     should be defined in just one file that will contain the fn definitions and variables

   USE_SNPRINTF 
     emulate snprintf on Win platforms (you will
     lose the security which is provided under unix of course)

   SOCK_ERRORS
     include error code handling and checking functions
*/

#pragma once

#ifndef __SISOCKS_H__
#define __SISOCKS_H__

#ifdef __cplusplus
extern "C" {
#endif

#if defined __GNUC__ && !defined unix && !defined WIN32 /* MacOS X hack (gcc on any platform should behave as unix - except for Win32, where we need to keep using winsock) */
#define unix
#endif

#if defined STANDALONE_RSERVE && defined RSERVE_PKG
#undef RSERVE_PKG
#endif

#include <stdio.h>
#include <string.h>

#ifdef unix
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
	
#define sockerrno errno
#define SOCKET int
#define INVALID_SOCKET (-1)
#define closesocket(A) close(A)
#define initsocks()
#define donesocks()
	
#else

#define windows
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#define inet_aton(A,B) (0, B.s_addr=inet_addr(A))

#define sockerrno WSAGetLastError()
int initsocks(void);
#define donesocks() WSACleanup()
	
#if !defined(_MSC_VER)
#ifndef WIN64
#define ECONNREFUSED WSAECONNREFUSED
#define EADDRINUSE WSAEADDRINUSE
#define ENOTSOCK WSAENOTSOCK
#define EISCONN WSAEISCONN
#define ETIMEDOUT WSAETIMEDOUT
#define ENETUNREACH WSAENETUNREACH
#define EINPROGRESS WSAEINPROGRESS
#define EALREADY WSAEALREADY
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#define EBADF WSAEBADF
#define EINVAL WSAEINVAL
#define EOPNOTSUPP WSAEOPNOTSUPP
#define EFAULT WSAEFAULT
#define EWOULDBLOCK WSAEWOULDBLOCK
#define EACCES WSAEACCES
#else
#define ECONNREFUSED WSAECONNREFUSED
#define EADDRINUSE WSAEADDRINUSE
#define ENOTSOCK WSAENOTSOCK
#define EISCONN WSAEISCONN
#define ETIMEDOUT WSAETIMEDOUT
#define ENETUNREACH WSAENETUNREACH
#define EINPROGRESS WSAEINPROGRESS
#define EALREADY WSAEALREADY
#define EOPNOTSUPP WSAEOPNOTSUPP
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#endif

#endif /* Windows */

#define SA struct sockaddr
#define SAIN struct sockaddr_in


#ifndef SISOCKS_C
extern int suppmode;
extern int socklasterr;
extern FILE *sockerrlog;
#endif

int sockerrorchecks(char *buf, size_t blen, int res);
int sockerrorcheck(char *sn, int rtb, int res);
    
#define FCF(X,F) sockerrorcheck(X,1,F)
#define CF(X,F) sockerrorcheck(X,0,F)


struct sockaddr *build_sin(struct sockaddr_in *sa,char *ip,int port);

#ifdef __cplusplus
} // extern "C"
#endif
	
#endif /* __SISOCKS_H__ */
