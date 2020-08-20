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
   Yunhong Gu, last updated 05/07/2011
*****************************************************************************/

#ifndef __UDT_CORE_H__
#define __UDT_CORE_H__

#define MAX_LOSS_RECORD 200
#define MAX_MONITOR 500

#include "api.h"
#include "buffer.h"
#include "cache.h"
#include "ccc.h"
#include "channel.h"
#include "common.h"
#include "list.h"
#include "packet.h"
#include "packet_tracker.h"
#include "queue.h"
#include "time.h"
#include "udt.h"
#include "window.h"
#include "../pcc/pcc_sender.h"
#include "../pcc/pcc_vivace_sender.h"
#include "../pcc/quic_types/quic_time.h"
#include "../pcc/quic_types/quic_types.h"

#include <deque>
#include <mutex>
#include <thread>
#include <vector>

typedef uint64_t PacketId;

enum UDTSockType {UDT_STREAM = 1, UDT_DGRAM};

class CUDT {
  friend class CUDTSocket;
  friend class CUDTUnited;
  friend class CCC;
  friend struct CUDTComp;
  friend class CCache<CInfoBlock>;
  friend class CRendezvousQueue;
  friend class CSndQueue;
  friend class CRcvQueue;
  friend class CSndUList;
  friend class CRcvUList;
  friend class PccSender;
  friend class PccVivaceSender;

 private:
  // constructor and desctructor
  CUDT();
  CUDT(const CUDT& ancestor);
  const CUDT& operator=(const CUDT&) {return *this;}
  ~CUDT();

 public:
  //API
  static int startup();
  static int cleanup();
  static UDTSOCKET socket(int af, int type = SOCK_STREAM, int protocol = 0);
  static int bind(UDTSOCKET u, const sockaddr* name, int namelen);
  static int bind(UDTSOCKET u, UDPSOCKET udpsock);
  static int listen(UDTSOCKET u, int backlog);
  static UDTSOCKET accept(UDTSOCKET u, sockaddr* addr, int* addrlen);
  static int connect(UDTSOCKET u, const sockaddr* name, int namelen);
  static int close(UDTSOCKET u);
  static int getpeername(UDTSOCKET u, sockaddr* name, int* namelen);
  static int getsockname(UDTSOCKET u, sockaddr* name, int* namelen);
  static int getsockopt(UDTSOCKET u,
                        int level,
                        UDTOpt optname,
                        void* optval,
                        int* optlen);
  static int setsockopt(UDTSOCKET u,
                        int level,
                        UDTOpt optname,
                        const void* optval,
                        int optlen);
  static int send(UDTSOCKET u, const char* buf, int len, int flags);
  static int recv(UDTSOCKET u, char* buf, int len, int flags);
  static int sendmsg(UDTSOCKET u,
                     const char* buf,
                     int len,
                     int ttl = -1,
                     bool inorder = false);
  static int recvmsg(UDTSOCKET u, char* buf, int len);
  static int64_t sendfile(UDTSOCKET u,
                          std::fstream& ifs,
                          int64_t& offset,
                          const int64_t& size,
                          const int& block = 364000);
  static int64_t recvfile(UDTSOCKET u,
                          std::fstream& ofs,
                          int64_t& offset,
                          const int64_t& size,
                          const int& block = 7280000);
  static int select(int nfds,
                    ud_set* readfds,
                    ud_set* writefds,
                    ud_set* exceptfds,
                    const timeval* timeout);
  static int selectEx(const std::vector<UDTSOCKET>& fds,
                      std::vector<UDTSOCKET>* readfds,
                      std::vector<UDTSOCKET>* writefds,
                      std::vector<UDTSOCKET>* exceptfds,
                      int64_t msTimeOut);
  static int epoll_create();
  static int epoll_add_usock(const int eid,
                             const UDTSOCKET u,
                             const int* events = NULL);
  static int epoll_add_ssock(const int eid,
                             const SYSSOCKET s,
                             const int* events = NULL);
  static int epoll_remove_usock(const int eid, const UDTSOCKET u);
  static int epoll_remove_ssock(const int eid, const SYSSOCKET s);
  static int epoll_wait(const int eid,
                        std::set<UDTSOCKET>* readfds,
                        std::set<UDTSOCKET>* writefds,
                        int64_t msTimeOut,
                        std::set<SYSSOCKET>* lrfds = NULL,
                        std::set<SYSSOCKET>* wrfds = NULL);
  static int epoll_release(const int eid);
  static CUDTException& getlasterror();
  static int perfmon(UDTSOCKET u, CPerfMon* perf, bool clear = true);
  static UDTSTATUS getsockstate(UDTSOCKET u);

  // internal API
  static CUDT* getUDTHandle(UDTSOCKET u);

 private:
  // Functionality:
  //    initialize a UDT entity and bind to a local address.
  // Parameters:
  //    None.
  // Returned value:
  //    None.
  void open();

  // Functionality:
  //    Start listening to any connection request.
  // Parameters:
  //    None.
  // Returned value:
  //    None.
  void listen();

  // Functionality:
  //    Connect to a UDT entity listening at address "peer".
  // Parameters:
  //    0) [in] peer: The address of the listening UDT entity.
  // Returned value:
  //    None.
  void connect(const sockaddr* peer);

  // Functionality:
  //    Process the response handshake packet.
  // Parameters:
  //    0) [in] pkt: handshake packet.
  // Returned value:
  //    Return 0 if connected, positive value if connection is in progress,
  //    otherwise error code.
   int connect(const CPacket& pkt) throw ();

  // Functionality:
  //    Connect to a UDT entity listening at address "peer", which has sent
  //    "hs" request.
  // Parameters:
  //    0) [in] peer: The address of the listening UDT entity.
  //    1) [in/out] hs: The handshake information sent by the peer side (in),
  //       negotiated value (out).
  // Returned value:
  //    None.
  void connect(const sockaddr* peer, CHandShake* hs);

    // Functionality:
    //    Close the opened UDT entity.
    // Parameters:
    //    None.
    // Returned value:
    //    None.
  void close();

  // Functionality:
  //    Request UDT to send out a data block "data" with size of "len".
  // Parameters:
  //    0) [in] data: The address of the application data to be sent.
  //    1) [in] len: The size of the data block.
  // Returned value:
  //    Actual size of data sent.
  int send(const char* data, const int& len);

  // Functionality:
  //    Request UDT to receive data to a memory block "data" with size of "len".
  // Parameters:
  //    0) [out] data: data received.
  //    1) [in] len: The desired size of data to be received.
  // Returned value:
  //    Actual size of data received.
  int recv(char* data, const int& len);

  // Functionality:
  //    send a message of a memory block "data" with size of "len".
  // Parameters:
  //    0) [out] data: data received.
  //    1) [in] len: The desired size of data to be received.
  //    2) [in] ttl: the time-to-live of the message.
  //    3) [in] inorder: if the message should be delivered in order.
  // Returned value:
  //    Actual size of data sent.
  int sendmsg(const char* data,
              const int& len,
              const int& ttl,
              const bool& inorder);

  // Functionality:
  //    Receive a message to buffer "data".
  // Parameters:
  //    0) [out] data: data received.
  //    1) [in] len: size of the buffer.
  // Returned value:
  //    Actual size of data received.
  int recvmsg(char* data, const int& len);

  // Functionality:
  //    Request UDT to send out a file described as "fd", starting from
  //    "offset", with size of "size".
  // Parameters:
  //    0) [in] ifs: The input file stream.
  //    1) [in, out] offset: From where to read and send data; output is the new
  //       offset when the call returns.
  //    2) [in] size: How many data to be sent.
  //    3) [in] block: size of block per read from disk
  // Returned value:
  //    Actual size of data sent.
  int64_t sendfile(std::fstream& ifs,
                   int64_t& offset,
                   const int64_t& size,
                   const int& block = 366000);

  // Functionality:
  //    Request UDT to receive data into a file described as "fd", starting from
  //    "offset", with expected size of "size".
  // Parameters:
  //    0) [out] ofs: The output file stream.
  //    1) [in, out] offset: From where to write data; output is the new offset
  //       when the call returns.
  //    2) [in] size: How many data to be received.
  //    3) [in] block: size of block per write to disk
  // Returned value:
  //    Actual size of data received.
  int64_t recvfile(std::fstream& ofs,
                   int64_t& offset,
                   const int64_t& size,
                   const int& block = 7320000);

  // Functionality:
  //    Configure UDT options.
  // Parameters:
  //    0) [in] optName: The enum name of a UDT option.
  //    1) [in] optval: The value to be set.
  //    2) [in] optlen: size of "optval".
  // Returned value:
  //    None.
  void setOpt(UDTOpt optName, const void* optval, const int& optlen);

  // Functionality:
  //    Read UDT options.
  // Parameters:
  //    0) [in] optName: The enum name of a UDT option.
  //    1) [in] optval: The value to be returned.
  //    2) [out] optlen: size of "optval".
  // Returned value:
  //    None.
  void getOpt(UDTOpt optName, void* optval, int& optlen);

  // Functionality:
  //    read the performance data since last sample() call.
  // Parameters:
  //    0) [in, out] perf: pointer to a CPerfMon structure to record the
  //       performance data.
  //    1) [in] clear: flag to decide if the local performance trace should be
  //       cleared.
  // Returned value:
  //    None.
  void sample(CPerfMon* perf, bool clear = true);

  // start monitor function
  // length: the length of the monitoration
  void start_monitor(int length);

  // end monitor function
  // utility: whether should we call utility function
  void end_monitor(bool call_utility);

  // check this loss happened in which monitor
  void monitor_loss(int loss);

  // check whether this ack/loss feedback end one monitor
  void check_monitor_end_ack(int ack);
  void check_monitor_end_loss(int loss);

  // add the seqNo into retransmission list which is used to
  // judge in which monitor the lost pkt has been sent
  void add_retransmission(int seqNo, int monitor);

  void reduce_retransmission_list(int ack);

  void resizeMSS(int mss);


  // monitor
  int current_monitor, previous_monitor, monitor_ttl;
  // int start_seq[MAX_MONITOR], start_retransmission[MAX_MONITOR],
  //     end_seq[MAX_MONITOR], end_retransmission[MAX_MONITOR];
  double start_time[MAX_MONITOR];
  double end_time[MAX_MONITOR];
  double end_transmission_time[MAX_MONITOR];
  // for state, 1=sending, 2= waiting, 3=finished
  int lost[MAX_MONITOR];
  int retransmission[MAX_MONITOR];
  int total[MAX_MONITOR];
  int new_transmission[MAX_MONITOR];
  int left[MAX_MONITOR];
  int state[MAX_MONITOR];
  int left_monitor;
  // int end_pkt[MAX_MONITOR];
  int32_t pkt_sending[MAX_MONITOR][8000];
  int32_t latency[MAX_MONITOR];
  vector<int32_t> loss_record1, loss_record2;
  vector<int32_t>::iterator itr_loss_record1, itr_loss_record2;
  int32_t latency_seq_start[MAX_MONITOR], latency_seq_end[MAX_MONITOR];
  int32_t latency_time_start[MAX_MONITOR], latency_time_end[MAX_MONITOR];
  int32_t time_interval[MAX_MONITOR];
  int lossptr;
  bool recv_ack[MAX_MONITOR][30000];
  int64_t latest_received_seq[MAX_MONITOR];
  uint64_t packet_space[MAX_MONITOR];
  uint64_t send_timestamp[MAX_MONITOR][30000];
  uint64_t rtt_trace[MAX_MONITOR][30000];
  int rtt_count[MAX_MONITOR];
  uint64_t rtt_value[MAX_MONITOR];
  bool monitor;
  int test;
  // int retransmission_list[60000], max_retransmission_list,
  //     min_retransmission_list_seqNo;

  // UDT global management base
  static CUDTUnited s_UDTUnited;

 public:
  // invalid socket descriptor
  static const UDTSOCKET INVALID_SOCK;
  // socket api error returned value
  static const int ERROR;

 private:
  // Identification
  // UDT socket number
  UDTSOCKET m_SocketID;
  // Type of the UDT connection (SOCK_STREAM or SOCK_DGRAM)
  UDTSockType m_iSockType;
  // peer id, for multiplexer
  UDTSOCKET m_PeerID;
  // UDT version, for compatibility use
  static const int m_iVersion;

 private:
  // Packet sizes
  // Maximum/regular packet size, in bytes
  int m_iPktSize;
  // Maximum/regular payload size, in bytes
  int m_iPayloadSize;
  // Maximum/regular payload size, in bytes
  int m_iRcvPayloadSize;

  // Options
  // Maximum Segment Size, in bytes
  int m_iMSS;
  // Sending syncronization mode
  bool m_bSynSending;
  // Receiving syncronization mode
  bool m_bSynRecving;
  // Maximum number of packets in flight from the peer side
  int m_iFlightFlagSize;
  // Maximum UDT sender buffer size
  int m_iSndBufSize;
  // Maximum UDT receiver buffer size
  int m_iRcvBufSize;
  // Linger information on close
  linger m_Linger;
  // UDP sending buffer size
  int m_iUDPSndBufSize;
  // UDP receiving buffer size
  int m_iUDPRcvBufSize;
  // IP version
  int m_iIPversion;
  // Rendezvous connection mode
  bool m_bRendezvous;
  // sending timeout in milliseconds
  int m_iSndTimeOut;
  // receiving timeout in milliseconds
  int m_iRcvTimeOut;
  // reuse an exiting port or not, for UDP multiplexer
  bool m_bReuseAddr;
  // maximum data transfer rate (threshold)
  int64_t m_llMaxBW;

  // congestion control
  // Factory class to create a specific CC instance
  CCCVirtualFactory* m_pCCFactory;
  // congestion control class
  CCC* m_pCC;
  PccSender* pcc_sender;
  PacketTracker<int32_t, PacketId>* packet_tracker_;
  // network information cache
  CCache<CInfoBlock>* m_pCache;

  // Status
  // If the UDT entit is listening to connection
  volatile bool m_bListening;
  // The short phase when connect() is called but not yet completed
  volatile bool m_bConnecting;
  // Whether the connection is on or off
  volatile bool m_bConnected;
  // If the UDT entity is closing
  volatile bool m_bClosing;
  // If the peer side has shutdown the connection
  volatile bool m_bShutdown;
  // If the connection has been broken
  volatile bool m_bBroken;
  // If the peer status is normal
  volatile bool m_bPeerHealth;
  // If the UDT entity has been opened
  bool m_bOpened;
  // a counter (number of GC checks) to let the GC tag this socket as
  // disconnected
  int m_iBrokenCounter;

  // Expiration counter
  int m_iEXPCount;
  // Estimated bandwidth, number of packets per second
  int m_iBandwidth;
  // RTT, in microseconds
  double m_iRTT;
  int last_rtt_;
  deque<double> m_last_rtt;
  static const size_t kRTTHistorySize = 100;
  // double m_last_rtt[MAX_MONITOR];
  int m_monitor_count;
  // RTT variance
  double m_iRTTVar;
  // Packet arrival rate at the receiver side
  int m_iDeliveryRate;

  // Linger expiration time (for GC to close a socket with data in sending
  // buffer)
  uint64_t m_ullLingerExpiration;

  // connection request
  CHandShake m_ConnReq;
  // connection response
  CHandShake m_ConnRes;
  // last time when a connection request is sent
  int64_t m_llLastReqTime;

  // Sending related data
  // Sender buffer
  CSndBuffer* m_pSndBuffer;
  // Sender loss list
  CSndLossList* m_pSndLossList;
  // Packet sending time window
  CPktTimeWindow* m_pSndTimeWindow;

  // aggregate difference in inter-packet time
  int64_t m_ullTimeDiff;

  // Flow control window size
  volatile int m_iFlowWindowSize;
  // congestion window size
  volatile double m_dCongestionWindow;

  // Last ACK received
  volatile int32_t m_iSndLastAck;
  // The real last ACK that updates the sender buffer and loss list
  volatile int32_t m_iSndLastDataAck;
  // The largest sequence number that has been sent
  volatile int32_t m_iSndCurrSeqNo;
  volatile int32_t m_iMonitorCurrSeqNo;
  // Sequence number sent last decrease occurs
  int32_t m_iLastDecSeq;
  // Last ACK2 sent back
  int32_t m_iSndLastAck2;
  // The time when last ACK2 was sent back
  uint64_t m_ullSndLastAck2Time;

  // Initial Sequence Number
  int32_t m_iISN;

  // Receiving related data
  // Receiver buffer
  CRcvBuffer* m_pRcvBuffer;
  // Receiver loss list
  CRcvLossList* m_pRcvLossList;
  // ACK history window
  CACKWindow* m_pACKWindow;
  // Packet arrival time window
  CPktTimeWindow* m_pRcvTimeWindow;

  // Last sent ACK
  int32_t m_iRcvLastAck;
  // Timestamp of last ACK
  uint64_t m_ullLastAckTime;
  // Last sent ACK that has been acknowledged
  int32_t m_iRcvLastAckAck;
  // Last ACK sequence number
  int32_t m_iAckSeqNo;
  // Largest received sequence number
  int32_t m_iRcvCurrSeqNo;

  // Last time that a warning message is sent
  uint64_t m_ullLastWarningTime;
  int32_t tsn_payload[1];
  // Initial Sequence Number of the peer side
  int32_t m_iPeerISN;

  // synchronization: mutexes and conditions
  // used to synchronize connection operation
  pthread_mutex_t m_ConnectionLock;

  // used to block "send" call
  pthread_cond_t m_SendBlockCond;
  // lock associated to m_SendBlockCond
  pthread_mutex_t m_SendBlockLock;

  // used to protected sender's loss list when processing ACK
  pthread_mutex_t m_AckLock;

  // used to block "recv" when there is no data
  pthread_cond_t m_RecvDataCond;
  // lock associated to m_RecvDataCond
  pthread_mutex_t m_RecvDataLock;

  // used to synchronize "send" call
  pthread_mutex_t m_SendLock;
  // used to synchronize "recv" call
  pthread_mutex_t m_RecvLock;

  pthread_mutex_t m_LossrecordLock;
  mutex monitor_mutex_;

  void initSynch();
  void destroySynch();
  void releaseSynch();
  double get_min_rtt() const;

  // Generation and processing of packets
  void SendAck(int32_t seq_no, int32_t msg_no);
  void sendCtrl(const int& pkttype,
                void* lparam = NULL,
                void* rparam = NULL,
                const int& size = 0);
  void ProcessAck(CPacket& ctrlpkt);
  void processCtrl(CPacket& ctrlpkt);
  uint64_t GetSendingInterval();
  int packData(CPacket& packet, uint64_t& ts);
  int processData(CUnit* unit);
  int listen(sockaddr* addr, CPacket& packet);
  void add_to_loss_record(int32_t loss1, int32_t loss2);
  uint64_t deadlines[MAX_MONITOR];
  uint64_t allocated_times_[MAX_MONITOR];
  int32_t GetNextSeqNo();

  int loss_head_loc;

  static const uint64_t kMinTimeoutMillis = 50000;
  // Trace
  // timestamp when the UDT entity is started
  uint64_t m_StartTime;
  // total number of sent data packets, including retransmissions
  int64_t m_llSentTotal;
  int64_t TotalBytes;
  int64_t BytesInFlight;
  // total number of received packets
  int64_t m_llRecvTotal;
  // total number of lost packets (sender side)
  int m_iSndLossTotal;
  // total number of lost packets (receiver side)
  int m_iRcvLossTotal;
  // total number of retransmitted packets
  int m_iRetransTotal;
  // total number of sent ACK packets
  int m_iSentACKTotal;
  // total number of received ACK packets
  int m_iRecvACKTotal;
  // total number of sent NAK packets
  int m_iSentNAKTotal;
  // total number of received NAK packets
  int m_iRecvNAKTotal;
  // total real time for sending
  int64_t m_llSndDurationTotal;

  // last performance sample time
  uint64_t m_LastSampleTime;
  // number of pakctes sent in the last trace interval
  int64_t m_llTraceSent;
  // number of pakctes received in the last trace interval
  int64_t m_llTraceRecv;
  // number of lost packets in the last trace interval (sender side)
  int m_iTraceSndLoss;
  // number of lost packets in the last trace interval (receiver side)
  int m_iTraceRcvLoss;
  // number of retransmitted packets in the last trace interval
  int m_iTraceRetrans;
  // number of ACKs sent in the last trace interval
  int m_iSentACK;
  // number of ACKs received in the last trace interval
  int m_iRecvACK;
  // number of NAKs sent in the last trace interval
  int m_iSentNAK;
  // number of NAKs received in the last trace interval
  int m_iRecvNAK;
  // real time for sending
  int64_t m_llSndDuration;
  // timers to record the sending duration
  int64_t m_llSndDurationCounter;

  // Timers
  // CPU clock frequency, used for Timer, ticks per microsecond
  uint64_t m_ullCPUFrequency;

  // Periodical Rate Control Interval, 10000 microsecond
  static const int m_iSYNInterval;
  // ACK interval for self-clocking
  static const int m_iSelfClockInterval;

  // Next ACK time, in CPU clock cycles, same below
  uint64_t m_ullNextACKTime;
  // Next NAK time
  uint64_t m_ullNextNAKTime;

  // SYN interval
  volatile uint64_t m_ullSYNInt;
  // ACK interval
  volatile uint64_t m_ullACKInt;
  // NAK interval
  volatile uint64_t m_ullNAKInt;
  // time stamp of last response from the peer
  volatile uint64_t m_ullLastRspTime;

  // NAK timeout lower bound;
  // too small value can cause unnecessary retransmission
  uint64_t m_ullMinNakInt;
  // timeout lower bound threshold: too small timeout can cause problem
  uint64_t m_ullMinExpInt;

  // packet counter for ACK
  int m_iPktCount;
  // light ACK counter
  int m_iLightACKCount;

  // scheduled time of next packet sending
  int64_t m_ullTargetTime;

  void checkTimers();

  // for UDP multiplexer
  // packet sending queue
  CSndQueue* m_pSndQueue;
  // packet receiving queue
  CRcvQueue* m_pRcvQueue;
  // peer address
  sockaddr* m_pPeerAddr;
  // local UDP IP address
  uint32_t m_piSelfIP[4];
  // node information for UDT list used in snd queue
  CSNode* m_pSNode;
  // node information for UDT list used in rcv queue
  CRNode* m_pRNode;

  void init_state();
  time_t start_;

  // for epoll
  // set of epoll ID to trigger
  std::set<int> m_sPollID;
  void addEPoll(const int eid);
  void removeEPoll(const int eid);
};


#endif
