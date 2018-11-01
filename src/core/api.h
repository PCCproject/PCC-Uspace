/*****************************************************************************
Copyright (c) 2001 - 2010, The Board of Trustees of the University of Illinois.
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
   Yunhong Gu, last updated 09/28/2010
*****************************************************************************/

#ifndef __UDT_API_H__
#define __UDT_API_H__

#include "cache.h"
#include "epoll.h"
#include "packet.h"
#include "queue.h"
#include "udt.h"

#include <map>
#include <vector>

class CUDT;

class CUDTSocket {
 public:
  CUDTSocket();
  ~CUDTSocket();

  // current socket state
  UDTSTATUS m_Status;

  // time when the socket is closed
  uint64_t m_TimeStamp;

  // IP version
  int m_iIPversion;
  // pointer to the local address of the socket
  sockaddr* m_pSelfAddr;
  // pointer to the peer address of the socket
  sockaddr* m_pPeerAddr;

  // socket ID
  UDTSOCKET m_SocketID;
  // ID of the listener socket; 0 means this is an independent socket
  UDTSOCKET m_ListenSocket;

  // peer socket ID
  UDTSOCKET m_PeerID;
  // initial sequence number, used to tell different connection from same
  // IP:port
  int32_t m_iISN;

  // pointer to the UDT entity
  CUDT* m_pUDT;

  // set of connections waiting for accept()
  std::set<UDTSOCKET>* m_pQueuedSockets;
  // set of accept()ed connections
  std::set<UDTSOCKET>* m_pAcceptSockets;

  // used to block "accept" call
  pthread_cond_t m_AcceptCond;
  // mutex associated to m_AcceptCond
  pthread_mutex_t m_AcceptLock;

  // maximum number of connections in queue
  unsigned int m_uiBackLog;

  // multiplexer ID
  int m_iMuxID;

  // lock this socket exclusively for control APIs: bind/listen/connect
  pthread_mutex_t m_ControlLock;

 private:
  CUDTSocket(const CUDTSocket&);
  CUDTSocket& operator=(const CUDTSocket&);
};

////////////////////////////////////////////////////////////////////////////////

class CUDTUnited {
  friend class CUDT;
  friend class CRendezvousQueue;

 public:
  CUDTUnited();
  ~CUDTUnited();

  // Functionality:
  //    initialize the UDT library.
  // Parameters:
  //    None.
  // Returned value:
  //    0 if success, otherwise -1 is returned.
  int startup();

  // Functionality:
  //    release the UDT library.
  // Parameters:
  //    None.
  // Returned value:
  //    0 if success, otherwise -1 is returned.
  int cleanup();

  // Functionality:
  //    Create a new UDT socket.
  // Parameters:
  //    0) [in] af: IP version, IPv4 (AF_INET) or IPv6 (AF_INET6).
  //    1) [in] type: socket type, SOCK_STREAM or SOCK_DGRAM
  // Returned value:
  //    The new UDT socket ID, or INVALID_SOCK.
  UDTSOCKET newSocket(const int& af, const int& type);

  // Functionality:
  //    Create a new UDT connection.
  // Parameters:
  //    0) [in] listen: the listening UDT socket;
  //    1) [in] peer: peer address.
  //    2) [in/out] hs: handshake information from peer side (in), negotiated
  //       value (out);
  // Returned value:
  //    If the new connection is successfully created: 1 success, 0 already
  //    exist, -1 error.
  int newConnection(const UDTSOCKET listen,
                    const sockaddr* peer,
                    CHandShake* hs);

  // Functionality:
  //    look up the UDT entity according to its ID.
  // Parameters:
  //    0) [in] u: the UDT socket ID.
  // Returned value:
  //    Pointer to the UDT entity.
  CUDT* lookup(const UDTSOCKET u);

  // Functionality:
  //    Check the status of the UDT socket.
  // Parameters:
  //    0) [in] u: the UDT socket ID.
  // Returned value:
  //    UDT socket status, or NONEXIST if not found.
  UDTSTATUS getStatus(const UDTSOCKET u);

  // socket APIs
  int bind(const UDTSOCKET u, const sockaddr* name, const int& namelen);
  int bind(const UDTSOCKET u, UDPSOCKET udpsock);
  int listen(const UDTSOCKET u, const int& backlog);
  UDTSOCKET accept(const UDTSOCKET listen, sockaddr* addr, int* addrlen);
  int connect(const UDTSOCKET u, const sockaddr* name, const int& namelen);
  int close(const UDTSOCKET u);
  int getpeername(const UDTSOCKET u, sockaddr* name, int* namelen);
  int getsockname(const UDTSOCKET u, sockaddr* name, int* namelen);
  int select(ud_set* readfds,
             ud_set* writefds,
             ud_set* exceptfds,
             const timeval* timeout);
  int selectEx(const std::vector<UDTSOCKET>& fds,
               std::vector<UDTSOCKET>* readfds,
               std::vector<UDTSOCKET>* writefds,
               std::vector<UDTSOCKET>* exceptfds,
               int64_t msTimeOut);
  int epoll_create();
  int epoll_add_usock(const int eid,
                      const UDTSOCKET u,
                      const int* events = NULL);
  int epoll_add_ssock(const int eid,
                      const SYSSOCKET s,
                      const int* events = NULL);
  int epoll_remove_usock(const int eid, const UDTSOCKET u);
  int epoll_remove_ssock(const int eid, const SYSSOCKET s);
  int epoll_wait(const int eid,
                 std::set<UDTSOCKET>* readfds,
                 std::set<UDTSOCKET>* writefds,
                 int64_t msTimeOut,
                 std::set<SYSSOCKET>* lrfds = NULL,
                 std::set<SYSSOCKET>* lwfds = NULL);
  int epoll_release(const int eid);

  // Functionality:
  //    record the UDT exception.
  // Parameters:
  //    0) [in] e: pointer to a UDT exception instance.
  // Returned value:
  //    None.
  void setError(CUDTException* e);

  // Functionality:
  //    look up the most recent UDT exception.
  // Parameters:
  //    None.
  // Returned value:
  //    pointer to a UDT exception instance.
  CUDTException* getError();

 private:
  // stores all the socket structures
  std::map<UDTSOCKET, CUDTSocket*> m_Sockets;

  // used to synchronize UDT API
  pthread_mutex_t m_ControlLock;

  // used to synchronize ID generation
  pthread_mutex_t m_IDLock;
  // seed to generate a new unique socket ID
  UDTSOCKET m_SocketID;

  // record sockets from peers to avoid repeated connection request,
  // int64_t = (socker_id << 30) + isn
  std::map<int64_t, std::set<UDTSOCKET> > m_PeerRec;

 private:
  // thread local error record (last error)
  pthread_key_t m_TLSError;
#ifndef WIN32
  static void TLSDestroy(void* e) { if (NULL != e) {delete (CUDTException*)e;} }
#else
  std::map<DWORD, CUDTException*> m_mTLSRecord;
  void checkTLSValue();
  pthread_mutex_t m_TLSLock;
#endif

 private:
  void connect_complete(const UDTSOCKET u);
  CUDTSocket* locate(const UDTSOCKET u);
  CUDTSocket* locate(const sockaddr* peer,
                     const UDTSOCKET& id,
                     const int32_t& isn);
  void updateMux(CUDTSocket* s,
                 const sockaddr* addr = NULL,
                 const UDPSOCKET* = NULL);
  void updateMux(CUDTSocket* s, const CUDTSocket* ls);

 private:
  // UDP multiplexer
  std::map<int, CMultiplexer> m_mMultiplexer;
  pthread_mutex_t m_MultiplexerLock;

 private:
  // UDT network information cache
  CCache<CInfoBlock>* m_pCache;

 private:
  volatile bool m_bClosing;
  pthread_mutex_t m_GCStopLock;
  pthread_cond_t m_GCStopCond;

  pthread_mutex_t m_InitLock;
  // number of startup() called by application
  int m_iInstanceCount;
  // if the GC thread is working (true)
  bool m_bGCStatus;

  pthread_t m_GCThread;
#ifndef WIN32
  static void* garbageCollect(void*);
#else
  static DWORD WINAPI garbageCollect(LPVOID);
#endif

  // temporarily store closed sockets
  std::map<UDTSOCKET, CUDTSocket*> m_ClosedSockets;

  void checkBrokenSockets();
  void removeSocket(const UDTSOCKET u);

 private:
  // handling epoll data structures and events
  CEPoll m_EPoll;

 private:
  CUDTUnited(const CUDTUnited&);
  CUDTUnited& operator=(const CUDTUnited&);
};

#endif
