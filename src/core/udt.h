/*****************************************************************************
Copyright (c) 2001 - 2011, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu, last updated 01/18/2011
*****************************************************************************/

#ifndef __UDT_H__
#define __UDT_H__

#include <fstream>
#include <set>
#include <string>
#include <vector>

#ifndef WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#else
#ifdef __MINGW__
#include <stdint.h>
#include <ws2tcpip.h>
#endif
#include <windows.h>
#endif


////////////////////////////////////////////////////////////////////////////////

// if compiling on VC6.0 or pre-WindowsXP systems
// use -DLEGACY_WIN32

// if compiling with MinGW, it only works on XP or above
// use -D_WIN32_WINNT=0x0501


#ifdef WIN32
#ifndef __MINGW__
// Explicitly define 32-bit and 64-bit numbers
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned __int32 uint32_t;
#ifndef LEGACY_WIN32
typedef unsigned __int64 uint64_t;
#else
// VC 6.0 does not support unsigned __int64: may cause potential problems.
typedef __int64 uint64_t;
#endif

#ifdef UDT_EXPORTS
#define UDT_API __declspec(dllexport)
#else
#define UDT_API __declspec(dllimport)
#endif
#else
#define UDT_API
#endif
#else
#define UDT_API
#endif

// #define NO_BUSY_WAITING

// #define DEBUG
#define EXPERIMENTAL_FEATURE_CONTINOUS_SEND
// #define INCAST
#ifdef WIN32
#ifndef __MINGW__
typedef SOCKET SYSSOCKET;
#else
typedef int SYSSOCKET;
#endif
#else
typedef int SYSSOCKET;
#endif

typedef SYSSOCKET UDPSOCKET;
typedef int UDTSOCKET;

////////////////////////////////////////////////////////////////////////////////

typedef std::set<UDTSOCKET> ud_set;
#define UD_CLR(u, uset) ((uset)->erase(u))
#define UD_ISSET(u, uset) ((uset)->find(u) != (uset)->end())
#define UD_SET(u, uset) ((uset)->insert(u))
#define UD_ZERO(uset) ((uset)->clear())

enum EPOLLOpt {
  // this values are defined same as linux epoll.h, so that if system values are
  // used by mistake, they should have the same effect
  UDT_EPOLL_IN = 0x1,
  UDT_EPOLL_OUT = 0x4,
  UDT_EPOLL_ERR = 0x8
};

enum UDTSTATUS {
  INIT = 1,
  OPENED,
  LISTENING,
  CONNECTING,
  CONNECTED,
  BROKEN,
  CLOSING,
  CLOSED,
  NONEXIST
};

////////////////////////////////////////////////////////////////////////////////

enum UDTOpt {
  // the Maximum Transfer Unit
  UDT_MSS,
  // if sending is blocking
  UDT_SNDSYN,
  // if receiving is blocking
  UDT_RCVSYN,
  // custom congestion control algorithm
  UDT_CC,
  // Flight flag size (window size)
  UDT_FC,
  // maximum buffer in sending queue
  UDT_SNDBUF,
  // UDT receiving buffer size
  UDT_RCVBUF,
  // waiting for unsent data when closing
  UDT_LINGER,
  // UDP sending buffer size
  UDP_SNDBUF,
  // UDP receiving buffer size
  UDP_RCVBUF,
  // maximum datagram message size
  UDT_MAXMSG,
  // time-to-live of a datagram message
  UDT_MSGTTL,
  // rendezvous connection mode
  UDT_RENDEZVOUS,
  // send() timeout
  UDT_SNDTIMEO,
  // recv() timeout
  UDT_RCVTIMEO,
  // reuse an existing port or create a new one
  UDT_REUSEADDR,
  // maximum bandwidth (bytes per second) that the connection can use
  UDT_MAXBW,
  // current socket state, see UDTSTATUS, read only
  UDT_STATE,
  // current avalable events associated with the socket
  UDT_EVENT,
  // size of data in the sending buffer
  UDT_SNDDATA,
  // size of data available for recv
  UDT_RCVDATA,
  // control algorithm for pcc sender
  UDT_PCC,
  // utility tag for pcc sender
  UDT_UTAG,
  // utility parameter for pcc sender
  UDT_UPARAM
};

////////////////////////////////////////////////////////////////////////////////

struct CPerfMon {
  // global measurements
  // time since the UDT entity is started, in milliseconds
  int64_t msTimeStamp;
  // total number of sent data packets, including retransmissions
  int64_t pktSentTotal;
  int64_t pktTotalBytes;
  // total number of received packets
  int64_t pktRecvTotal;
  // total number of lost packets (sender side)
  int pktSndLossTotal;
  // total number of lost packets (receiver side)
  int pktRcvLossTotal;
  // total number of retransmitted packets
  int pktRetransTotal;
  // total number of sent ACK packets
  int pktSentACKTotal;
  // total number of received ACK packets
  int pktRecvACKTotal;
  // total number of sent NAK packets
  int pktSentNAKTotal;
  // total number of received NAK packets
  int pktRecvNAKTotal;
  // total time duration when UDT is sending data (idle time exclusive)
  int64_t usSndDurationTotal;

  // local measurements
  // number of sent data packets, including retransmissions
  int64_t pktSent;
  // number of received packets
  int64_t pktRecv;
  // number of lost packets (sender side)
  int pktSndLoss;
  // number of lost packets (receiver side)
  int pktRcvLoss;
  // number of retransmitted packets
  int pktRetrans;
  // number of sent ACK packets
  int pktSentACK;
  // number of received ACK packets
  int pktRecvACK;
  // number of sent NAK packets
  int pktSentNAK;
  // number of received NAK packets
  int pktRecvNAK;
  // sending rate in Mb/s
  double mbpsSendRate;
  // receiving rate in Mb/s
  double mbpsRecvRate;
  // busy sending time (i.e., idle time exclusive)
  int64_t usSndDuration;
  // busy sending time (i.e., idle time exclusive)
  double mbpsGoodput;

  // instant measurements
  // packet sending period, in microseconds
  double usPktSndPeriod;
  // flow window size, in number of packets
  int pktFlowWindow;
  // congestion window size, in number of packets
  int pktCongestionWindow;
  // number of packets on flight
  int pktFlightSize;
  // RTT, in milliseconds
  double msRTT;
  // estimated bandwidth, in Mb/s
  double mbpsBandwidth;
  // available UDT sender buffer size
  int byteAvailSndBuf;
  // available UDT receiver buffer size
  int byteAvailRcvBuf;
};

////////////////////////////////////////////////////////////////////////////////

class UDT_API CUDTException {
 public:
  CUDTException(int major = 0, int minor = 0, int err = -1);
  CUDTException(const CUDTException& e);
  virtual ~CUDTException();

  // Functionality:
  //    Get the description of the exception.
  // Parameters:
  //    None.
  // Returned value:
  //    Text message for the exception description.
  virtual const char* getErrorMessage();

  // Functionality:
  //    Get the system errno for the exception.
  // Parameters:
  //    None.
  // Returned value:
  //    errno.
  virtual int getErrorCode() const;

  // Functionality:
  //    Clear the error code.
  // Parameters:
  //    None.
  // Returned value:
  //    None.
  virtual void clear();

 private:
  // major exception categories
  int m_iMajor;

  // 0: correct condition
  // 1: network setup exception
  // 2: network connection broken
  // 3: memory exception
  // 4: file exception
  // 5: method not supported
  // 6+: undefined error

  // for specific error reasons
  int m_iMinor;
  // errno returned by the system if there is any
  int m_iErrno;
  // text error message
  std::string m_strMsg;

  // the name of UDT function that returns the error
  std::string m_strAPI;
  // debug information, set to the original place that causes the error
  std::string m_strDebug;

 public:
  // Error Code
  static const int SUCCESS;
  static const int ECONNSETUP;
  static const int ENOSERVER;
  static const int ECONNREJ;
  static const int ESOCKFAIL;
  static const int ESECFAIL;
  static const int ECONNFAIL;
  static const int ECONNLOST;
  static const int ENOCONN;
  static const int ERESOURCE;
  static const int ETHREAD;
  static const int ENOBUF;
  static const int EFILE;
  static const int EINVRDOFF;
  static const int ERDPERM;
  static const int EINVWROFF;
  static const int EWRPERM;
  static const int EINVOP;
  static const int EBOUNDSOCK;
  static const int ECONNSOCK;
  static const int EINVPARAM;
  static const int EINVSOCK;
  static const int EUNBOUNDSOCK;
  static const int ENOLISTEN;
  static const int ERDVNOSERV;
  static const int ERDVUNBOUND;
  static const int ESTREAMILL;
  static const int EDGRAMILL;
  static const int EDUPLISTEN;
  static const int ELARGEMSG;
  static const int EINVPOLLID;
  static const int EASYNCFAIL;
  static const int EASYNCSND;
  static const int EASYNCRCV;
  static const int EPEERERR;
  static const int EUNKNOWN;
};

////////////////////////////////////////////////////////////////////////////////

namespace UDT {
typedef CUDTException ERRORINFO;
typedef UDTOpt SOCKOPT;
typedef CPerfMon TRACEINFO;
typedef ud_set UDSET;

UDT_API extern const UDTSOCKET INVALID_SOCK;
#undef ERROR
UDT_API extern const int ERROR;

UDT_API int startup();
UDT_API int cleanup();
UDT_API UDTSOCKET socket(int af, int type, int protocol);
UDT_API int bind(UDTSOCKET u, const struct sockaddr* name, int namelen);
UDT_API int bind(UDTSOCKET u, UDPSOCKET udpsock);
UDT_API int listen(UDTSOCKET u, int backlog);
UDT_API UDTSOCKET accept(UDTSOCKET u, struct sockaddr* addr, int* addrlen);
UDT_API int connect(UDTSOCKET u, const struct sockaddr* name, int namelen);
UDT_API int close(UDTSOCKET u);
UDT_API int getpeername(UDTSOCKET u, struct sockaddr* name, int* namelen);
UDT_API int getsockname(UDTSOCKET u, struct sockaddr* name, int* namelen);
UDT_API int getsockopt(UDTSOCKET u,
                       int level,
                       SOCKOPT optname,
                       void* optval,
                       int* optlen);
UDT_API int setsockopt(UDTSOCKET u,
                       int level,
                       SOCKOPT optname,
                       const void* optval,
                       int optlen);
UDT_API int send(UDTSOCKET u, const char* buf, int len, int flags);
UDT_API int recv(UDTSOCKET u, char* buf, int len, int flags);
UDT_API int sendmsg(UDTSOCKET u,
                    const char* buf,
                    int len,
                    int ttl = -1,
                    bool inorder = false);
UDT_API int recvmsg(UDTSOCKET u, char* buf, int len);
UDT_API int64_t sendfile(UDTSOCKET u,
                         std::fstream& ifs,
                         int64_t& offset,
                         int64_t size,
                         int block = 364000);
UDT_API int64_t recvfile(UDTSOCKET u,
                         std::fstream& ofs,
                         int64_t& offset,
                         int64_t size,
                         int block = 7280000);
UDT_API int select(int nfds,
                   UDSET* readfds,
                   UDSET* writefds,
                   UDSET* exceptfds,
                   const struct timeval* timeout);
UDT_API int selectEx(const std::vector<UDTSOCKET>& fds,
                     std::vector<UDTSOCKET>* readfds,
                     std::vector<UDTSOCKET>* writefds,
                     std::vector<UDTSOCKET>* exceptfds,
                     int64_t msTimeOut);
UDT_API int epoll_create();
UDT_API int epoll_add_usock(const int eid,
                            const UDTSOCKET u,
                            const int* events = NULL);
UDT_API int epoll_add_ssock(const int eid,
                            const SYSSOCKET s,
                            const int* events = NULL);
UDT_API int epoll_remove_usock(const int eid, const UDTSOCKET u);
UDT_API int epoll_remove_ssock(const int eid, const SYSSOCKET s);
UDT_API int epoll_wait(const int eid,
                       std::set<UDTSOCKET>* readfds,
                       std::set<UDTSOCKET>* writefds,
                       int64_t msTimeOut,
                       std::set<SYSSOCKET>* lrfds = NULL,
                       std::set<SYSSOCKET>* wrfds = NULL);
UDT_API int epoll_release(const int eid);
UDT_API ERRORINFO& getlasterror();
UDT_API int perfmon(UDTSOCKET u, TRACEINFO* perf, bool clear = true);
UDT_API UDTSTATUS getsockstate(UDTSOCKET u);
}

#endif
