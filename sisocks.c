#define SISOCKS_C
#include "sisocks.h"
#include <errno.h>

#ifdef windows

int initsocks(void)
{
	WSADATA dt;
	/* initialize WinSock 1.1 */
	return (WSAStartup(0x0101,&dt))?-1:0;
}

#endif


int suppmode=0;
int socklasterr;
FILE *sockerrlog=0;

/* copy error description to buf or set *buf=0 if none */
int sockerrorchecks(char *buf, size_t blen, int res) {
	*buf=0;
	if (res==-1) {
		switch(sockerrno) {
			case EBADF: strncpy(buf,"bad descriptor",blen); break;
			case EINVAL: strncpy(buf,"already in use",blen); break;
			case EACCES: strncpy(buf,"access denied",blen); break;
			case ENOTSOCK: strncpy(buf,"descriptor is not a socket",blen); break;
			case EOPNOTSUPP: strncpy(buf,"operation not supported",blen); break;
			case EFAULT: strncpy(buf,"fault",blen); break;
			case EWOULDBLOCK: strncpy(buf,"operation would block",blen); break;
			case EISCONN: strncpy(buf,"is already connected",blen); break;
			case ECONNREFUSED: strncpy(buf,"connection refused",blen); break;
			case ETIMEDOUT: strncpy(buf,"operation timed out",blen); break;
			case ENETUNREACH: strncpy(buf,"network is unreachable",blen); break;
			case EADDRINUSE: strncpy(buf,"address already in use",blen); break;
			case EINPROGRESS: strncpy(buf,"in progress",blen); break;
			case EALREADY: strncpy(buf,"previous connect request not completed yet",blen); break;
			#ifdef unix
			default: snprintf(buf,blen,"unknown socket error %d",sockerrno);
			#else
			default: sprintf(buf,"unknown socket error %d",sockerrno);
			#endif
		}
	}
	return res;
}

#ifdef RSERVE_PKG /* use error instead of exit */

#include <R.h>

int sockerrorcheck(char *sn, int rtb, int res) {
	if ((signed int)res == -1) {
		char sock_err_buf[72];
		sockerrorchecks(sock_err_buf, sizeof(sock_err_buf), res);
		if (rtb)
			Rf_error("%s socket error #%d (%s)", sn, (int) sockerrno, sock_err_buf);
		else
			Rf_warning("%s socket error #%d (%s)", sn, (int) sockerrno, sock_err_buf);
	}
	return res;
}

#else

/* check socket error and add to log file if necessary */
int sockerrorcheck(char *sn, int rtb, int res) {
	if (!sockerrlog) sockerrlog=stderr;
	if ((signed int)res==-1) {
		if (socklasterr==sockerrno) {
			suppmode++;
		} else {
			if (suppmode>0) {
				fprintf(sockerrlog,"##> REP: (last error has been repeated %d times.)\n",suppmode);
				suppmode=0;
			}
			fprintf(sockerrlog,"##> SOCK_ERROR: %s error #%d",sn,sockerrno);
			switch(sockerrno) {
				case EBADF: fprintf(sockerrlog,"(bad descriptor)"); break;
				case EINVAL: fprintf(sockerrlog,"(already in use)"); break;
				case EACCES: fprintf(sockerrlog,"(access denied)"); break;
				case ENOTSOCK: fprintf(sockerrlog,"(descriptor is not a socket)"); break;
				case EOPNOTSUPP: fprintf(sockerrlog,"(operation not supported)"); break;
				case EFAULT: fprintf(sockerrlog,"(fault)"); break;
				case EWOULDBLOCK: fprintf(sockerrlog,"(operation would block)"); break;
				case EISCONN: fprintf(sockerrlog,"(is already connected)"); break;
				case ECONNREFUSED: fprintf(sockerrlog,"(connection refused)"); break;
				case ETIMEDOUT: fprintf(sockerrlog,"(operation timed out)"); break;
				case ENETUNREACH: fprintf(sockerrlog,"(network is unreachable)"); break;
				case EADDRINUSE: fprintf(sockerrlog,"(address already in use)"); break;
				case EINPROGRESS: fprintf(sockerrlog,"(in progress)"); break;
				case EALREADY: fprintf(sockerrlog,"(previous connect request not completed yet)"); break;
				default: fprintf(sockerrlog,"(?)");
			}
			fprintf(sockerrlog,"\n"); fflush(sockerrlog);
			socklasterr=sockerrno;
		}
		if (rtb) exit(1);
	}
	return res;
}

#endif /* RSERVE_PKG */


struct sockaddr *build_sin(struct sockaddr_in *sa,char *ip,int port) {
	memset(sa,0,sizeof(struct sockaddr_in));
	sa->sin_family=AF_INET;
	sa->sin_port=htons(port);
	sa->sin_addr.s_addr=(ip)?inet_addr(ip):htonl(INADDR_ANY);
	return (struct sockaddr*)sa;
}
