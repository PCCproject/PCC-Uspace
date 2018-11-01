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
   Yunhong Gu, last updated 01/12/2011
*****************************************************************************/


#ifndef __UDT_QUEUE_H__
#define __UDT_QUEUE_H__

#include "channel.h"
#include "common.h"
#include "packet.h"

#include <list>
#include <map>
#include <queue>
#include <vector>

class CUDT;

struct CUnit {
  // packet
  CPacket m_Packet;
  // 0: free, 1: occupied, 2: msg read but not freed (out-of-order),
  // 3: msg dropped
  int m_iFlag;
};

class CUnitQueue {
  friend class CRcvQueue;
  friend class CRcvBuffer;

 public:
  CUnitQueue();
  ~CUnitQueue();

  // Functionality:
  //    Initialize the unit queue.
  // Parameters:
  //    1) [in] size: queue size
  //    2) [in] mss: maximum segament size
  //    3) [in] version: IP version
  // Returned value:
  //    0: success, -1: failure.
  int init(const int& size, const int& mss, const int& version);

  // Functionality:
  //    Increase (double) the unit queue size.
  // Parameters:
  //    None.
  // Returned value:
  //    0: success, -1: failure.
  int increase();

  // Functionality:
  //    Decrease (halve) the unit queue size.
  // Parameters:
  //    None.
  // Returned value:
  //    0: success, -1: failure.
  int shrink();

  // Functionality:
  //    find an available unit for incoming packet.
  // Parameters:
  //    None.
  // Returned value:
  //    Pointer to the available unit, NULL if not found.
  CUnit* getNextAvailUnit();

 private:
  struct CQEntry {
    // unit queue
    CUnit* m_pUnit;
    // data buffer
    char* m_pBuffer;
    // size of each queue
    int m_iSize;

    CQEntry* m_pNext;
  }

  // pointer to the first unit queue
  *m_pQEntry,
  // pointer to the current available queue
  *m_pCurrQueue,
  // pointer to the last unit queue
  *m_pLastQueue;

  // recent available unit
  CUnit* m_pAvailUnit;

  // total size of the unit queue, in number of packets
  int m_iSize;
  // total number of valid packets in the queue
  int m_iCount;

  // unit buffer size
  int m_iMSS;
  // IP version
  int m_iIPversion;

  CUnitQueue(const CUnitQueue&);
  CUnitQueue& operator=(const CUnitQueue&);
};

struct CSNode {
  // Pointer to the instance of CUDT socket
  CUDT* m_pUDT;
  // Time Stamp
  uint64_t m_llTimeStamp;

  // location on the heap, -1 means not on the heap
  int m_iHeapLoc;
};

class CSndUList {
  friend class CSndQueue;

 public:
  CSndUList();
  ~CSndUList();

  // Functionality:
  //    Insert a new UDT instance into the list.
  // Parameters:
  //    1) [in] ts: time stamp: next processing time
  //    2) [in] u: pointer to the UDT instance
  // Returned value:
  //    None.
  void insert(const int64_t& ts, const CUDT* u);

  // Functionality:
  //    Update the timestamp of the UDT instance on the list.
  // Parameters:
  //    1) [in] u: pointer to the UDT instance
  //    2) [in] resechedule: if the timestampe shoudl be rescheduled
  // Returned value:
  //    None.
  void update(const CUDT* u, const bool& reschedule = true);

  // Functionality:
  //    Retrieve the next packet and peer address from the first entry, and
  //    reschedule it in the queue.
  // Parameters:
  //    0) [out] addr: destination address of the next packet
  //    1) [out] pkt: the next packet to be sent
  // Returned value:
  //    1 if successfully retrieved, -1 if no packet found.
  int pop(sockaddr*& addr, CPacket& pkt);

  // Functionality:
  //    Remove UDT instance from the list.
  // Parameters:
  //    1) [in] u: pointer to the UDT instance
  // Returned value:
  //    None.
  void remove(const CUDT* u);

  // Functionality:
  //    Retrieve the next scheduled processing time.
  // Parameters:
  //    None.
  // Returned value:
  //    Scheduled processing time of the first UDT socket in the list.
  uint64_t getNextProcTime();

 private:
  void insert_(const int64_t& ts, const CUDT* u);
  void remove_(const CUDT* u);

  // The heap array
  CSNode** m_pHeap;
  // physical length of the array
  int m_iArrayLength;
  // position of last entry on the heap array
  int m_iLastEntry;

  pthread_mutex_t m_ListLock;

  pthread_mutex_t* m_pWindowLock;
  pthread_cond_t* m_pWindowCond;

  CTimer* m_pTimer;

  CSndUList(const CSndUList&);
  CSndUList& operator=(const CSndUList&);
};

struct CRNode {
  // Pointer to the instance of CUDT socket
  CUDT* m_pUDT;
  // Time Stamp
  uint64_t m_llTimeStamp;

  // previous link
  CRNode* m_pPrev;
  // next link
  CRNode* m_pNext;

  // if the node is already on the list
  bool m_bOnList;
};

class CRcvUList {
 public:
  CRcvUList();
  ~CRcvUList();

  // Functionality:
  //    Insert a new UDT instance to the list.
  // Parameters:
  //    1) [in] u: pointer to the UDT instance
  // Returned value:
  //    None.
  void insert(const CUDT* u);

  // Functionality:
  //    Remove the UDT instance from the list.
  // Parameters:
  //    1) [in] u: pointer to the UDT instance
  // Returned value:
  //    None.
  void remove(const CUDT* u);

  // Functionality:
  //    Move the UDT instance to the end of the list, if it already exists;
  //    otherwise, do nothing.
  // Parameters:
  //    1) [in] u: pointer to the UDT instance
  // Returned value:
  //    None.
  void update(const CUDT* u);

  // the head node
  CRNode* m_pUList;

 private:
  // the last node
  CRNode* m_pLast;

  CRcvUList(const CRcvUList&);
  CRcvUList& operator=(const CRcvUList&);
};

class CHash {
 public:
  CHash();
  ~CHash();

  // Functionality:
  //    Initialize the hash table.
  // Parameters:
  //    1) [in] size: hash table size
  // Returned value:
  //    None.
  void init(const int& size);

  // Functionality:
  //    Look for a UDT instance from the hash table.
  // Parameters:
  //    1) [in] id: socket ID
  // Returned value:
  //    Pointer to a UDT instance, or NULL if not found.
  CUDT* lookup(const int32_t& id);

  // Functionality:
  //    Insert an entry to the hash table.
  // Parameters:
  //    1) [in] id: socket ID
  //    2) [in] u: pointer to the UDT instance
  // Returned value:
  //    None.
  void insert(const int32_t& id, const CUDT* u);

  // Functionality:
  //    Remove an entry from the hash table.
  // Parameters:
  //    1) [in] id: socket ID
  // Returned value:
  //    None.
  void remove(const int32_t& id);

 private:
  // list of buckets (the hash table)
  struct CBucket {
    // Socket ID
    int32_t m_iID;
    // Socket instance
    CUDT* m_pUDT;

    // next bucket
    CBucket* m_pNext;
  } **m_pBucket;

  // size of hash table
  int m_iHashSize;

  CHash(const CHash&);
  CHash& operator=(const CHash&);
};

class CRendezvousQueue {
 public:
  CRendezvousQueue();
  ~CRendezvousQueue();

  void insert(const UDTSOCKET& id,
              CUDT* u,
              const int& ipv,
              const sockaddr* addr,
              const uint64_t& ttl);
  void remove(const UDTSOCKET& id);
  CUDT* retrieve(const sockaddr* addr, UDTSOCKET& id);

  void updateConnStatus();

 private:
  struct CRL {
    // UDT socket ID (self)
    UDTSOCKET m_iID;
    // UDT instance
    CUDT* m_pUDT;
    // IP version
    int m_iIPversion;
    // UDT sonnection peer address
    sockaddr* m_pPeerAddr;
    // the time that this request expires
    uint64_t m_ullTTL;
  };
  // The sockets currently in rendezvous mode
  std::list<CRL> m_lRendezvousID;

  pthread_mutex_t m_RIDVectorLock;
};

class CSndQueue {
  friend class CUDT;
  friend class CUDTUnited;

 public:
  CSndQueue();
  ~CSndQueue();

  // Functionality:
  //    Initialize the sending queue.
  // Parameters:
  //    1) [in] c: UDP channel to be associated to the queue
  //    2) [in] t: Timer
  // Returned value:
  //    None.
  void init(const CChannel* c, const CTimer* t);

  // Functionality:
  //    Send out a packet to a given address.
  // Parameters:
  //    1) [in] addr: destination address
  //    2) [in] packet: packet to be sent out
  // Returned value:
  //    Size of data sent out.
  int sendto(const sockaddr* addr, CPacket& packet);

 private:
#ifndef WIN32
  static void* worker(void* param);
#else
  static DWORD WINAPI worker(LPVOID param);
#endif

  pthread_t m_WorkerThread;

  // List of UDT instances for data sending
  CSndUList* m_pSndUList;
  // The UDP channel for data sending
  CChannel* m_pChannel;
  // Timing facility
  CTimer* m_pTimer;

  pthread_mutex_t m_WindowLock;
  pthread_cond_t m_WindowCond;


  // closing the worker
  volatile bool m_bClosing;
  pthread_cond_t m_ExitCond;

  CSndQueue(const CSndQueue&);
  CSndQueue& operator=(const CSndQueue&);
};

class CRcvQueue {
  friend class CUDT;
  friend class CUDTUnited;

 public:
  CRcvQueue();
  ~CRcvQueue();

  // Functionality:
  //    Initialize the receiving queue.
  // Parameters:
  //    1) [in] size: queue size
  //    2) [in] mss: maximum packet size
  //    3) [in] version: IP version
  //    4) [in] hsize: hash table size
  //    5) [in] c: UDP channel to be associated to the queue
  //    6) [in] t: timer
  // Returned value:
  //    None.
  void init(const int& size, const int& payload, const int& version, const int& hsize, const CChannel* c, const CTimer* t);

  // Functionality:
  //    Read a packet for a specific UDT socket id.
  // Parameters:
  //    1) [in] id: Socket ID
  //    2) [out] packet: received packet
  // Returned value:
  //    Data size of the packet
  int recvfrom(const int32_t& id, CPacket& packet);

 private:
#ifndef WIN32
  static void* worker(void* param);
#else
  static DWORD WINAPI worker(LPVOID param);
#endif

  pthread_t m_WorkerThread;

  // The received packet queue
  CUnitQueue m_UnitQueue;

  // List of UDT instances that will read packets from the queue
  CRcvUList* m_pRcvUList;
  // Hash table for UDT socket looking up
  CHash* m_pHash;
  // UDP channel for receving packets
  CChannel* m_pChannel;
  // shared timer with the snd queue
  CTimer* m_pTimer;

  // packet payload size
  int m_iPayloadSize;

  // closing the workder
  volatile bool m_bClosing;
  pthread_cond_t m_ExitCond;

  int setListener(const CUDT* u);
  void removeListener(const CUDT* u);

  void registerConnector(const UDTSOCKET& id,
                         CUDT* u,
                         const int& ipv,
                         const sockaddr* addr,
                         const uint64_t& ttl);
  void removeConnector(const UDTSOCKET& id);

  void setNewEntry(CUDT* u);
  bool ifNewEntry();
  CUDT* getNewEntry();

  void storePkt(const int32_t& id, CPacket* pkt);

  pthread_mutex_t m_LSLock;
  // pointer to the (unique, if any) listening UDT entity
  volatile CUDT* m_pListener;
  // The list of sockets in rendezvous mode
  CRendezvousQueue* m_pRendezvousQueue;

  // newly added entries, to be inserted
  std::vector<CUDT*> m_vNewEntry;
  pthread_mutex_t m_IDLock;

  // temporary buffer for rendezvous connection request
  std::map<int32_t, std::queue<CPacket*> > m_mBuffer;
  pthread_mutex_t m_PassLock;
  pthread_cond_t m_PassCond;

  CRcvQueue(const CRcvQueue&);
  CRcvQueue& operator=(const CRcvQueue&);
};

struct CMultiplexer {
  // The sending queue
  CSndQueue* m_pSndQueue;
  // The receiving queue
  CRcvQueue* m_pRcvQueue;
  // The UDP channel for sending and receiving
  CChannel* m_pChannel;
  // The timer
  CTimer* m_pTimer;

  // The UDP port number of this multiplexer
  int m_iPort;
  // IP version
  int m_iIPversion;
  // Maximum Segment Size
  int m_iMSS;
  // number of UDT instances that are associated with this multiplexer
  int m_iRefCount;
  // if this one can be shared with others
  bool m_bReusable;

  // multiplexer ID
  int m_iID;
};

#endif
