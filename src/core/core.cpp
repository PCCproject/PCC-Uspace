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

//#define DEBUG_CORE_SENDING_RATE

#ifndef WIN32
#include <assert.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <vector>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef LEGACY_WIN32
#include <wspiapi.h>
#endif
#endif
#include <cmath>
#include <sstream>
#include <iostream>
#include "queue.h"
#include "core.h"
#include <unordered_map>
#include <map>
#include <mutex>

//#define DEBUG_SEND_SEQ_AND_ID
//#define DEBUG_LOSS

namespace std {
    int operator==(const struct timespec& ts1, const struct timespec& ts2) {
            return ts1.tv_sec == ts2.tv_sec && ts1.tv_nsec == ts2.tv_nsec;
    }
}
using namespace std;

std::mutex pcc_sender_lock;

CUDTUnited CUDT::s_UDTUnited;

const UDTSOCKET CUDT::INVALID_SOCK = -1;
const int CUDT::ERROR = -1;

const UDTSOCKET UDT::INVALID_SOCK = CUDT::INVALID_SOCK;
const int UDT::ERROR = CUDT::ERROR;

const int32_t CSeqNo::m_iSeqNoTH = 0x3FFFFFFF;
const int32_t CSeqNo::m_iMaxSeqNo = 0x7FFFFFFF;
const int32_t CAckNo::m_iMaxAckSeqNo = 0x7FFFFFFF;
const int32_t CMsgNo::m_iMsgNoTH = 0xFFFFFFF;
const int32_t CMsgNo::m_iMaxMsgNo = 0x1FFFFFFF;

const int CUDT::m_iVersion = 4;
const int CUDT::m_iSYNInterval = 1000000;
const int CUDT::m_iSelfClockInterval = 64;


CUDT::CUDT()
{
	m_pSndBuffer = NULL;
	m_pRcvBuffer = NULL;
	m_pSndLossList = NULL;
	m_pRcvLossList = NULL;
	m_pACKWindow = NULL;
	m_pSndTimeWindow = NULL;
	m_pRcvTimeWindow = NULL;

	m_pSndQueue = NULL;
	m_pRcvQueue = NULL;
	m_pPeerAddr = NULL;
	m_pSNode = NULL;
	m_pRNode = NULL;

	// Initilize mutex and condition variables
	initSynch();


	// Default UDT configurations
	m_iMSS = 1500;
	m_bSynSending = true;
	m_bSynRecving = true;
	m_iFlightFlagSize = 1000000;
	m_iSndBufSize =100000;
	m_iRcvBufSize = 1000000; //Rcv buffer MUST NOT be bigger than Flight Flag size
	m_Linger.l_onoff = 1;
	m_Linger.l_linger = 180;
	m_iUDPSndBufSize = 100000;
	m_iUDPRcvBufSize = m_iRcvBufSize * m_iMSS;
	m_iSockType = UDT_STREAM;
	m_iIPversion = AF_INET;
	m_bRendezvous = false;
	m_iSndTimeOut = -1;
	m_iRcvTimeOut = -1;
	m_bReuseAddr = true;
	lossptr=0;
	m_llMaxBW = -1;
	m_llLastReqTime = CTimer::getTime();

	m_pCCFactory = new CCCFactory<CUDTCC>;
	m_pCC = m_pCCFactory->create();
	m_pCache = NULL;

    pcc_sender = new PccSender(10000, 10, 10);
	packet_tracker_ = new PacketTracker<int32_t, PacketId>(&m_SendBlockCond);

	// Initial status
	m_bOpened = false;
	m_bListening = false;
	m_bConnecting = false;
	m_bConnected = false;
	m_bClosing = false;
	m_bShutdown = false;
	m_bBroken = false;
	m_bPeerHealth = true;
	m_ullLingerExpiration = 0;
	start_ = time(NULL);
	remove( "/home/yossi/timeout_times.txt" );
	for (int i = 0; i < MAX_MONITOR; i++) state[i] = 0;
}

CUDT::CUDT(const CUDT& ancestor)
{
	m_pSndBuffer = NULL;
	m_pRcvBuffer = NULL;
	m_pSndLossList = NULL;
	m_pRcvLossList = NULL;
	m_pACKWindow = NULL;
	m_pSndTimeWindow = NULL;
	m_pRcvTimeWindow = NULL;

	m_pSndQueue = NULL;
	m_pRcvQueue = NULL;
	m_pPeerAddr = NULL;
	m_pSNode = NULL;
	m_pRNode = NULL;

	// Initilize mutex and condition variables
	initSynch();

	// Default UDT configurations
	m_iMSS = ancestor.m_iMSS;
	m_bSynSending = ancestor.m_bSynSending;
	m_bSynRecving = ancestor.m_bSynRecving;
	m_iFlightFlagSize = ancestor.m_iFlightFlagSize;
	m_iSndBufSize = ancestor.m_iSndBufSize;
	m_iRcvBufSize = ancestor.m_iRcvBufSize;
	m_Linger = ancestor.m_Linger;
	m_iUDPSndBufSize = ancestor.m_iUDPSndBufSize;
	m_iUDPRcvBufSize = ancestor.m_iUDPRcvBufSize;
	m_iSockType = ancestor.m_iSockType;
	m_iIPversion = ancestor.m_iIPversion;
	m_bRendezvous = ancestor.m_bRendezvous;
	m_iSndTimeOut = ancestor.m_iSndTimeOut;
	m_iRcvTimeOut = ancestor.m_iRcvTimeOut;
	m_bReuseAddr = true;	// this must be true, because all accepted sockets shared the same port with the listener
	m_llMaxBW = ancestor.m_llMaxBW;
	m_llLastReqTime = CTimer::getTime();

	m_pCCFactory = ancestor.m_pCCFactory->clone();
	m_pCache = ancestor.m_pCache;
    if (m_pCC != NULL) {
        delete m_pCC;
    }
	m_pCC = m_pCCFactory->create();

    pcc_sender = new PccSender(10000, 10, 10);
	packet_tracker_ = new PacketTracker<int32_t, PacketId>(&m_SendBlockCond);

	// Initial status
	m_bOpened = false;
	m_bListening = false;
	m_bConnecting = false;
	m_bConnected = false;
	m_bClosing = false;
	m_bShutdown = false;
	m_bBroken = false;
	m_bPeerHealth = true;
	m_ullLingerExpiration = 0;
	start_ = time(NULL);
	remove( "/home/yossi/timeout_times.txt" );
	for (int i = 0; i < MAX_MONITOR; i++) state[i] = 0;
}

CUDT::~CUDT()
{

	// release mutex/condtion variables
	destroySynch();

	// destroy the data structures
	delete m_pSndBuffer;
	delete m_pRcvBuffer;
	delete m_pSndLossList;
	delete m_pRcvLossList;
	delete m_pACKWindow;
	delete m_pSndTimeWindow;
	delete m_pRcvTimeWindow;
	delete m_pCCFactory;
	delete m_pCC;
	delete m_pPeerAddr;
	delete m_pSNode;
	delete m_pRNode;
    delete pcc_sender;
    delete packet_tracker_;
}

void CUDT::setOpt(UDTOpt optName, const void* optval, const int&)
{
	if (m_bBroken || m_bClosing)
		throw CUDTException(2, 1, 0);

	CGuard cg(m_ConnectionLock);
	CGuard sendguard(m_SendLock);
	CGuard recvguard(m_RecvLock);

	switch (optName)
	{
	case UDT_MSS:
		if (m_bOpened)
			throw CUDTException(5, 1, 0);

		if (*(int*)optval < int(28 + CHandShake::m_iContentSize))
			throw CUDTException(5, 3, 0);

		m_iMSS = *(int*)optval;

		// Packet size cannot be greater than UDP buffer size
		if (m_iMSS > m_iUDPSndBufSize)
			m_iMSS = m_iUDPSndBufSize;
		if (m_iMSS > m_iUDPRcvBufSize)
			m_iMSS = m_iUDPRcvBufSize;

		break;

	case UDT_SNDSYN:
		m_bSynSending = *(bool *)optval;
		break;

	case UDT_RCVSYN:
		m_bSynRecving = *(bool *)optval;
		break;

	case UDT_CC:
		if (m_bConnecting || m_bConnected)
			throw CUDTException(5, 1, 0);
		if (NULL != m_pCCFactory)
			delete m_pCCFactory;
		m_pCCFactory = ((CCCVirtualFactory *)optval)->clone();

		break;

	case UDT_FC:
		if (m_bConnecting || m_bConnected)
			throw CUDTException(5, 2, 0);

		if (*(int*)optval < 1)
			throw CUDTException(5, 3);

		// Mimimum recv flight flag size is 32 packets
		if (*(int*)optval > 32)
			m_iFlightFlagSize = *(int*)optval;
		else
			m_iFlightFlagSize = 32;

		break;

	case UDT_SNDBUF:
		if (m_bOpened)
			throw CUDTException(5, 1, 0);

		if (*(int*)optval <= 0)
			throw CUDTException(5, 3, 0);

		m_iSndBufSize = *(int*)optval / (m_iMSS - 28);

		break;

	case UDT_RCVBUF:
		if (m_bOpened)
			throw CUDTException(5, 1, 0);

		if (*(int*)optval <= 0)
			throw CUDTException(5, 3, 0);

		// Mimimum recv buffer size is 32 packets
		if (*(int*)optval > (m_iMSS - 28) * 32)
			m_iRcvBufSize = *(int*)optval / (m_iMSS - 28);
		else
			m_iRcvBufSize = 32;

		// recv buffer MUST not be greater than FC size
		if (m_iRcvBufSize > m_iFlightFlagSize)
			m_iRcvBufSize = m_iFlightFlagSize;

		break;

	case UDT_LINGER:
		m_Linger = *(linger*)optval;
		break;

	case UDP_SNDBUF:
		if (m_bOpened)
			throw CUDTException(5, 1, 0);

		m_iUDPSndBufSize = *(int*)optval;

		if (m_iUDPSndBufSize < m_iMSS)
			m_iUDPSndBufSize = m_iMSS;

		break;

	case UDP_RCVBUF:
		if (m_bOpened)
			throw CUDTException(5, 1, 0);

		m_iUDPRcvBufSize = *(int*)optval;

		if (m_iUDPRcvBufSize < m_iMSS)
			m_iUDPRcvBufSize = m_iMSS;

		break;

	case UDT_RENDEZVOUS:
		if (m_bConnecting || m_bConnected)
			throw CUDTException(5, 1, 0);
		m_bRendezvous = *(bool *)optval;
		break;

	case UDT_SNDTIMEO:
		m_iSndTimeOut = *(int*)optval;
		break;

	case UDT_RCVTIMEO:
		m_iRcvTimeOut = *(int*)optval;
		break;

	case UDT_REUSEADDR:
		if (m_bOpened)
			throw CUDTException(5, 1, 0);
		m_bReuseAddr = *(bool*)optval;
		break;

	case UDT_MAXBW:
		if (m_bConnecting || m_bConnected)
			throw CUDTException(5, 1, 0);
		m_llMaxBW = *(int64_t*)optval;
		break;

	default:
		throw CUDTException(5, 0, 0);
	}
}

void CUDT::getOpt(UDTOpt optName, void* optval, int& optlen)
{
	CGuard cg(m_ConnectionLock);

	switch (optName)
	{
	case UDT_MSS:
		*(int*)optval = m_iMSS;
		optlen = sizeof(int);
		break;

	case UDT_SNDSYN:
		*(bool*)optval = m_bSynSending;
		optlen = sizeof(bool);
		break;

	case UDT_RCVSYN:
		*(bool*)optval = m_bSynRecving;
		optlen = sizeof(bool);
		break;

	case UDT_CC:
		if (!m_bOpened)
			throw CUDTException(5, 5, 0);
		*(CCC**)optval = m_pCC;
		optlen = sizeof(CCC*);

		break;

	case UDT_FC:
		*(int*)optval = m_iFlightFlagSize;
		optlen = sizeof(int);
		break;

	case UDT_SNDBUF:
		*(int*)optval = m_iSndBufSize * (m_iMSS - 28);
		optlen = sizeof(int);
		break;

	case UDT_RCVBUF:
		*(int*)optval = m_iRcvBufSize * (m_iMSS - 28);
		optlen = sizeof(int);
		break;

	case UDT_LINGER:
		if (optlen < (int)(sizeof(linger)))
			throw CUDTException(5, 3, 0);

		*(linger*)optval = m_Linger;
		optlen = sizeof(linger);
		break;

	case UDP_SNDBUF:
		*(int*)optval = m_iUDPSndBufSize;
		optlen = sizeof(int);
		break;

	case UDP_RCVBUF:
		*(int*)optval = m_iUDPRcvBufSize;
		optlen = sizeof(int);
		break;

	case UDT_RENDEZVOUS:
		*(bool *)optval = m_bRendezvous;
		optlen = sizeof(bool);
		break;

	case UDT_SNDTIMEO:
		*(int*)optval = m_iSndTimeOut;
		optlen = sizeof(int);
		break;

	case UDT_RCVTIMEO:
		*(int*)optval = m_iRcvTimeOut;
		optlen = sizeof(int);
		break;

	case UDT_REUSEADDR:
		*(bool *)optval = m_bReuseAddr;
		optlen = sizeof(bool);
		break;

	case UDT_MAXBW:
		*(int64_t*)optval = m_llMaxBW;
		optlen = sizeof(int64_t);
		break;

	case UDT_STATE:
		*(int32_t*)optval = s_UDTUnited.getStatus(m_SocketID);
		optlen = sizeof(int32_t);
		break;

	case UDT_EVENT:
	{
		int32_t event = 0;
		if (m_bBroken)
			event |= UDT_EPOLL_ERR;
		else
		{
			if (m_pRcvBuffer && (m_pRcvBuffer->getRcvDataSize() > 0))
				event |= UDT_EPOLL_IN;
			if (m_pSndBuffer && (m_iSndBufSize > m_pSndBuffer->getCurrBufSize()))
				event |= UDT_EPOLL_OUT;
		}
		*(int32_t*)optval = event;
		optlen = sizeof(int32_t);
		break;
	}

	case UDT_SNDDATA:
		if (m_pSndBuffer)
			*(int32_t*)optval = m_pSndBuffer->getCurrBufSize();
		else
			*(int32_t*)optval = 0;
		optlen = sizeof(int32_t);
		break;

	case UDT_RCVDATA:
		if (m_pRcvBuffer)
			*(int32_t*)optval = m_pRcvBuffer->getRcvDataSize();
		else
			*(int32_t*)optval = 0;
		optlen = sizeof(int32_t);
		break;

	default:
		throw CUDTException(5, 0, 0);
	}
}

void CUDT::open()
{
	CGuard cg(m_ConnectionLock);

	// Initial sequence number, loss, acknowledgement, etc.
	m_iPktSize = m_iMSS - 28;
	m_iPayloadSize = m_iPktSize - CPacket::m_iPktHdrSize;
	m_iRcvPayloadSize = m_iPktSize - CPacket::m_iPktHdrSize;

	m_iEXPCount = 1;
	m_iBandwidth = 1;
	m_iDeliveryRate = 16;
	m_iAckSeqNo = 0;
	m_ullLastAckTime = 0;

	// trace information
	m_StartTime = CTimer::getTime();
	TotalBytes = m_llSentTotal = m_llRecvTotal = m_iSndLossTotal = m_iRcvLossTotal = m_iRetransTotal = m_iSentACKTotal = m_iRecvACKTotal = m_iSentNAKTotal = m_iRecvNAKTotal = 0;
	m_LastSampleTime = CTimer::getTime();
	m_llTraceSent = m_llTraceRecv = m_iTraceSndLoss = m_iTraceRcvLoss = m_iTraceRetrans = m_iSentACK = m_iRecvACK = m_iSentNAK = m_iRecvNAK = 0;
	m_llSndDuration = m_llSndDurationTotal = 0;

	// structures for queue
	if (NULL == m_pSNode)
		m_pSNode = new CSNode;
	m_pSNode->m_pUDT = this;
	m_pSNode->m_llTimeStamp = 1;
	m_pSNode->m_iHeapLoc = -1;

	if (NULL == m_pRNode)
		m_pRNode = new CRNode;
	m_pRNode->m_pUDT = this;
	m_pRNode->m_llTimeStamp = 1;
	m_pRNode->m_pPrev = m_pRNode->m_pNext = NULL;
	m_pRNode->m_bOnList = false;

	m_iRTT = 10 * m_iSYNInterval;
	last_rtt_ = 10 * m_iSYNInterval;
	//for (int i = 0; i < MAX_MONITOR; i++) m_last_rtt[i] = 5 * m_iSYNInterval;
	m_iRTTVar = m_iRTT / 2.0;
	m_ullCPUFrequency = CTimer::getCPUFrequency();

	// set up the imers
	m_ullSYNInt = m_iSYNInterval * m_ullCPUFrequency;

	// set minimum NAK and EXP timeout to 100ms
	m_ullMinNakInt = 410000 * m_ullCPUFrequency;
	m_ullMinExpInt = 410000 * m_ullCPUFrequency;

	m_ullACKInt = m_ullSYNInt;
	m_ullNAKInt = m_ullMinNakInt;

	uint64_t currtime;
	CTimer::rdtsc(currtime);
	m_ullLastRspTime = currtime;
	m_ullNextACKTime = currtime + m_ullSYNInt;
	m_ullNextNAKTime = currtime + m_ullNAKInt;

	m_iPktCount = 0;
	m_iLightACKCount = 1;

	m_ullTargetTime = 0;
	m_ullTimeDiff = 0;

	// Now UDT is opened.
	m_bOpened = true;
}

void CUDT::listen()
{
	CGuard cg(m_ConnectionLock);

	if (!m_bOpened)
		throw CUDTException(5, 0, 0);

	if (m_bConnecting || m_bConnected)
		throw CUDTException(5, 2, 0);

	// listen can be called more than once
	if (m_bListening)
		return;

	// if there is already another socket listening on the same port
	if (m_pRcvQueue->setListener(this) < 0)
		throw CUDTException(5, 11, 0);

	m_bListening = true;
}

void CUDT::connect(const sockaddr* serv_addr)
{
	std::cout << "connect" << std::endl;
	CGuard cg(m_ConnectionLock);

	if (!m_bOpened)
		throw CUDTException(5, 0, 0);

	if (m_bListening)
		throw CUDTException(5, 2, 0);

	if (m_bConnecting || m_bConnected)
		throw CUDTException(5, 2, 0);

	// record peer/server address
	delete m_pPeerAddr;
	m_pPeerAddr = (AF_INET == m_iIPversion) ? (sockaddr*)new sockaddr_in : (sockaddr*)new sockaddr_in6;
	memcpy(m_pPeerAddr, serv_addr, (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6));

	// register this socket in the rendezvous queue
	// RendezevousQueue is used to temporarily store incoming handshake, non-rendezvous connections also require this function
	uint64_t ttl = 3000000;
	if (m_bRendezvous)
		ttl *= 10;
	ttl += CTimer::getTime();
	m_pRcvQueue->registerConnector(m_SocketID, this, m_iIPversion, serv_addr, ttl);

	// This is my current configurations
	m_ConnReq.m_iVersion = m_iVersion;
	m_ConnReq.m_iType = m_iSockType;
	m_ConnReq.m_iMSS = m_iMSS;
	m_ConnReq.m_iFlightFlagSize = (m_iRcvBufSize < m_iFlightFlagSize)? m_iRcvBufSize : m_iFlightFlagSize;
	m_ConnReq.m_iReqType = (!m_bRendezvous) ? 1 : 0;
	m_ConnReq.m_iID = m_SocketID;
	CIPAddress::ntop(serv_addr, m_ConnReq.m_piPeerIP, m_iIPversion);

	// Random Initial Sequence Number
	srand((unsigned int)CTimer::getTime());
	m_iISN = m_ConnReq.m_iISN = (int32_t)(CSeqNo::m_iMaxSeqNo * (double(rand()) / RAND_MAX));
	m_iLastDecSeq = m_iISN - 1;
	m_iSndLastAck = m_iISN;
	m_iSndLastDataAck = m_iISN;
	m_iSndCurrSeqNo = m_iISN - 1;
	m_iSndLastAck2 = m_iISN;
	m_iMonitorCurrSeqNo=0;
	m_ullSndLastAck2Time = CTimer::getTime();

	// Inform the server my configurations.
	CPacket request;
	char* reqdata = new char [m_iPayloadSize];
	request.pack(0, NULL, reqdata, m_iPayloadSize);
	// ID = 0, connection request
	request.m_iID = 0;

	int hs_size = m_iPayloadSize;
	m_ConnReq.serialize(reqdata, hs_size);
	request.setLength(hs_size);
	m_pSndQueue->sendto(serv_addr, request);
	m_llLastReqTime = CTimer::getTime();

	m_bConnecting = true;

	// asynchronous connect, return immediately
	if (!m_bSynRecving)
	{
		delete [] reqdata;
		return;
	}

	// Wait for the negotiated configurations from the peer side.
	CPacket response;
	char* resdata = new char [m_iPayloadSize];
	response.pack(0, NULL, resdata, m_iPayloadSize);

	CUDTException e(0, 0);

	while (!m_bClosing)
	{
		// avoid sending too many requests, at most 1 request per 250ms
		if (CTimer::getTime() - m_llLastReqTime > 250000)
		{
			m_ConnReq.serialize(reqdata, hs_size);
			request.setLength(hs_size);
			if (m_bRendezvous)
				request.m_iID = m_ConnRes.m_iID;
			m_pSndQueue->sendto(serv_addr, request);
			m_llLastReqTime = CTimer::getTime();
		}

		response.setLength(m_iPayloadSize);
		if (m_pRcvQueue->recvfrom(m_SocketID, response) > 0)
		{
			if (connect(response) <= 0)
				break;

			// new request/response should be sent out immediately on receving a response
			m_llLastReqTime = 0;
		}

		if (CTimer::getTime() > ttl)
		{
			// timeout
			e = CUDTException(1, 1, 0);
			break;
		}
	}

	delete [] reqdata;
	delete [] resdata;

	if (e.getErrorCode() == 0)
	{
		if (m_bClosing)                                                 // if the socket is closed before connection...
			e = CUDTException(1);
		else if (1002 == m_ConnRes.m_iReqType)                          // connection request rejected
			e = CUDTException(1, 2, 0);
		else if ((!m_bRendezvous) && (m_iISN != m_ConnRes.m_iISN))      // secuity check
			e = CUDTException(1, 4, 0);
	}

	if (e.getErrorCode() != 0)
		throw e;
	std::cout << "finished connect" << std::endl;
}

int CUDT::connect(const CPacket& response) throw ()
				{
	// this is the 2nd half of a connection request. If the connection is setup successfully this returns 0.
	// returning -1 means there is an error.
	// returning 1 or 2 means the connection is in process and needs more handshake
	//cout<<"this is the 2nd connection\n";
	if (!m_bConnecting)
		return -1;

	if (m_bRendezvous && ((0 == response.getFlag()) || (1 == response.getType())) && (0 != m_ConnRes.m_iType))
	{
		//a data packet or a keep-alive packet comes, which means the peer side is already connected
		// in this situation, the previously recorded response will be used
		goto POST_CONNECT;
	}

	if ((1 != response.getFlag()) || (0 != response.getType()))
		return -1;

	m_ConnRes.deserialize(response.m_pcData, response.getLength());

	if (m_bRendezvous)
	{
		// regular connect should NOT communicate with rendezvous connect
		// rendezvous connect require 3-way handshake
		if (1 == m_ConnRes.m_iReqType)
			return -1;

		if ((0 == m_ConnReq.m_iReqType) || (0 == m_ConnRes.m_iReqType))
		{
			m_ConnReq.m_iReqType = -1;
			// the request time must be updated so that the next handshake can be sent out immediately.
			m_llLastReqTime = 0;
			return 1;
		}
	}
	else
	{
		// set cookie
		if (1 == m_ConnRes.m_iReqType)
		{
			m_ConnReq.m_iReqType = -1;
			m_ConnReq.m_iCookie = m_ConnRes.m_iCookie;
			m_llLastReqTime = 0;
			return 1;
		}
	}

	POST_CONNECT:
	// Remove from rendezvous queue
	m_pRcvQueue->removeConnector(m_SocketID);

	// Re-configure according to the negotiated values.
	m_iMSS = m_ConnRes.m_iMSS;
	m_iFlowWindowSize = m_ConnRes.m_iFlightFlagSize;
	m_iPktSize = m_iMSS - 28;
	m_iPayloadSize = m_iPktSize - CPacket::m_iPktHdrSize;
	m_iRcvPayloadSize = m_iPktSize - CPacket::m_iPktHdrSize;
	m_iPeerISN = m_ConnRes.m_iISN;
	m_iRcvLastAck = m_ConnRes.m_iISN;
	m_iRcvLastAckAck = m_ConnRes.m_iISN;
	m_iRcvCurrSeqNo = m_ConnRes.m_iISN - 1;
	m_PeerID = m_ConnRes.m_iID;
	memcpy(m_piSelfIP, m_ConnRes.m_piPeerIP, 16);

	// Prepare all data structures
	try
	{
		m_pSndBuffer = new CSndBuffer(64, m_iPayloadSize);
		m_pRcvBuffer = new CRcvBuffer(&(m_pRcvQueue->m_UnitQueue), m_iRcvBufSize);
		// after introducing lite ACK, the sndlosslist may not be cleared in time, so it requires twice space.
		m_pSndLossList = new CSndLossList(m_iFlowWindowSize * 2);
		m_pRcvLossList = new CRcvLossList(m_iFlightFlagSize);
		m_pACKWindow = new CACKWindow(1024);
		m_pRcvTimeWindow = new CPktTimeWindow(16, 64);
		m_pSndTimeWindow = new CPktTimeWindow();
	}
	catch (...)
	{
		throw CUDTException(3, 2, 0);
	}

	CInfoBlock ib;
	ib.m_iIPversion = m_iIPversion;
	CInfoBlock::convert(m_pPeerAddr, m_iIPversion, ib.m_piIP);
	if (m_pCache->lookup(&ib) >= 0)
	{
		m_iRTT = ib.m_iRTT;
		m_iBandwidth = ib.m_iBandwidth;
	}

	m_pCC = m_pCCFactory->create();
    m_pCC->m_UDT = m_SocketID;
	m_pCC->setMSS(m_iMSS);
	m_pCC->setMaxCWndSize((int&)m_iFlowWindowSize);
	m_pCC->setSndCurrSeqNo((int32_t&)m_iSndCurrSeqNo);
	m_pCC->setRcvRate(m_iDeliveryRate);
	m_pCC->setRTT(m_iRTT);
	m_pCC->setBandwidth(m_iBandwidth);
	if (m_llMaxBW > 0)
		m_pCC->setUserParam((char*)&(m_llMaxBW), 8);
	m_pCC->init();

	m_dCongestionWindow = m_pCC->m_dCWndSize;

	// And, I am connected too.
	m_bConnecting = false;
	m_bConnected = true;

	// register this socket for receiving data packets
	m_pRNode->m_bOnList = true;
	m_pRcvQueue->setNewEntry(this);

	// acknowledde any waiting epolls to write
	s_UDTUnited.m_EPoll.enable_write(m_SocketID, m_sPollID);

	// acknowledge the management module.
	s_UDTUnited.connect_complete(m_SocketID);
	//cout<<"finishied connection\n";

	return 0;
				}

void CUDT::connect(const sockaddr* peer, CHandShake* hs)
{
	CGuard cg(m_ConnectionLock);

	// Uses the smaller MSS between the peers
	if (hs->m_iMSS > m_iMSS)
		hs->m_iMSS = m_iMSS;
	else
		m_iMSS = hs->m_iMSS;

	// exchange info for maximum flow window size
	m_iFlowWindowSize = hs->m_iFlightFlagSize;
	//cout<<m_iFlowWindowSize<<"DING"<<endl;
	hs->m_iFlightFlagSize = (m_iRcvBufSize < m_iFlightFlagSize)? m_iRcvBufSize : m_iFlightFlagSize;

	m_iPeerISN = hs->m_iISN;

	m_iRcvLastAck = hs->m_iISN;
	m_iRcvLastAckAck = hs->m_iISN;
	m_iRcvCurrSeqNo = hs->m_iISN - 1;

	m_PeerID = hs->m_iID;
	hs->m_iID = m_SocketID;

	// use peer's ISN and send it back for security check
	m_iISN = hs->m_iISN;

	m_iLastDecSeq = m_iISN - 1;
	m_iSndLastAck = m_iISN;
	m_iSndLastDataAck = m_iISN;
	m_iSndCurrSeqNo = m_iISN - 1;
	m_iSndLastAck2 = m_iISN;
	m_ullSndLastAck2Time = CTimer::getTime();

	// this is a reponse handshake
	hs->m_iReqType = -1;

	// get local IP address and send the peer its IP address (because UDP cannot get local IP address)
	memcpy(m_piSelfIP, hs->m_piPeerIP, 16);
	CIPAddress::ntop(peer, hs->m_piPeerIP, m_iIPversion);

	m_iPktSize = m_iMSS - 28;
	m_iPayloadSize = m_iPktSize - CPacket::m_iPktHdrSize;
	m_iRcvPayloadSize = m_iPktSize - CPacket::m_iPktHdrSize;

	// Prepare all structures
	try
	{
		m_pSndBuffer = new CSndBuffer(64, m_iPayloadSize);
		m_pRcvBuffer = new CRcvBuffer(&(m_pRcvQueue->m_UnitQueue), m_iRcvBufSize);
		m_pSndLossList = new CSndLossList(m_iFlowWindowSize * 2);
		m_pRcvLossList = new CRcvLossList(m_iFlightFlagSize);
		m_pACKWindow = new CACKWindow(1024);
		m_pRcvTimeWindow = new CPktTimeWindow(16, 64);
		m_pSndTimeWindow = new CPktTimeWindow();
	}
	catch (...)
	{
		throw CUDTException(3, 2, 0);
	}

	CInfoBlock ib;
	ib.m_iIPversion = m_iIPversion;
	CInfoBlock::convert(peer, m_iIPversion, ib.m_piIP);
	if (m_pCache->lookup(&ib) >= 0)
	{
		m_iRTT = ib.m_iRTT;
		m_iBandwidth = ib.m_iBandwidth;
	}

	m_pCC = m_pCCFactory->create();
	m_pCC->m_UDT = m_SocketID;
	m_pCC->setMSS(m_iMSS);
	m_pCC->setMaxCWndSize((int&)m_iFlowWindowSize);
	m_pCC->setSndCurrSeqNo((int32_t&)m_iSndCurrSeqNo);
	m_pCC->setRcvRate(m_iDeliveryRate);
	m_pCC->setRTT(m_iRTT);
	m_pCC->setBandwidth(m_iBandwidth);
	if (m_llMaxBW > 0) m_pCC->setUserParam((char*)&(m_llMaxBW), 8);
	m_pCC->init();

	m_dCongestionWindow = m_pCC->m_dCWndSize;

	m_pPeerAddr = (AF_INET == m_iIPversion) ? (sockaddr*)new sockaddr_in : (sockaddr*)new sockaddr_in6;
	memcpy(m_pPeerAddr, peer, (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6));

	// And of course, it is connected.
	m_bConnected = true;

	// register this socket for receiving data packets
	m_pRNode->m_bOnList = true;
	m_pRcvQueue->setNewEntry(this);

	//send the response to the peer, see listen() for more discussions about this
	CPacket response;
	int size = CHandShake::m_iContentSize;
	char* buffer = new char[size];
	hs->serialize(buffer, size);
	response.pack(0, NULL, buffer, size);
	response.m_iID = m_PeerID;
	m_pSndQueue->sendto(peer, response);
	delete [] buffer;
}

void CUDT::close()
{
	if (!m_bOpened)
		return;

	if (0 != m_Linger.l_onoff)
	{
		uint64_t entertime = CTimer::getTime();

		while (!m_bBroken && m_bConnected && (m_pSndBuffer->getCurrBufSize() > 0) && (CTimer::getTime() - entertime < m_Linger.l_linger * 1000000ULL))
		{
			// linger has been checked by previous close() call and has expired
			if (m_ullLingerExpiration >= entertime)
				break;

			if (!m_bSynSending)
			{
				// if this socket enables asynchronous sending, return immediately and let GC to close it later
				if (0 == m_ullLingerExpiration)
					m_ullLingerExpiration = entertime + m_Linger.l_linger * 1000000ULL;

				return;
			}

#ifndef WIN32
			timespec ts;
			ts.tv_sec = 0;
			ts.tv_nsec = 1000000;
			nanosleep(&ts, NULL);
#else
			Sleep(1);
#endif
		}
	}

	// remove this socket from the snd queue
	if (m_bConnected)
		m_pSndQueue->m_pSndUList->remove(this);

	// remove itself from all epoll monitoring
	try
	{
		for (set<int>::iterator i = m_sPollID.begin(); i != m_sPollID.end(); ++ i)
			s_UDTUnited.m_EPoll.remove_usock(*i, m_SocketID);
	}
	catch (...)
	{
	}

	if (!m_bOpened)
		return;

	// Inform the threads handler to stop.
	m_bClosing = true;

	CGuard cg(m_ConnectionLock);

	// Signal the sender and recver if they are waiting for data.
	releaseSynch();

	if (m_bListening)
	{
		m_bListening = false;
		m_pRcvQueue->removeListener(this);
	}
	else
	{
		m_pRcvQueue->removeConnector(m_SocketID);
	}

	if (m_bConnected)
	{
		if (!m_bShutdown)
			sendCtrl(5);

		m_pCC->close();

		CInfoBlock ib;
		ib.m_iIPversion = m_iIPversion;
		CInfoBlock::convert(m_pPeerAddr, m_iIPversion, ib.m_piIP);
		ib.m_iRTT = m_iRTT;
		ib.m_iBandwidth = m_iBandwidth;
		m_pCache->update(&ib);

		m_bConnected = false;
	}

	// waiting all send and recv calls to stop
	CGuard sendguard(m_SendLock);
	CGuard recvguard(m_RecvLock);

	// CLOSED.
	m_bOpened = false;
}

int32_t CUDT::GetNextSeqNo() {
    m_iSndCurrSeqNo = CSeqNo::incseq(m_iSndCurrSeqNo);
    return m_iSndCurrSeqNo;
}

int CUDT::send(const char* data, const int& len)
{
    if (UDT_DGRAM == m_iSockType)
		throw CUDTException(5, 10, 0);

	// throw an exception if not connected
	if (m_bBroken || m_bClosing)
		throw CUDTException(2, 1, 0);
	else if (!m_bConnected)
		throw CUDTException(2, 2, 0);

	if (len <= 0)
		return 0;
	CGuard sendguard(m_SendLock);

	if (!packet_tracker_->CanEnqueuePacket())
	{
		if (!m_bSynSending)
			throw CUDTException(6, 1, 0);
		else
		{
			// wait here during a blocking sending
#ifndef WIN32
			pthread_mutex_lock(&m_SendBlockLock);
			if (m_iSndTimeOut < 0)
			{
				while (!m_bBroken && m_bConnected && !m_bClosing && !packet_tracker_->CanEnqueuePacket() && m_bPeerHealth)
					pthread_cond_wait(&m_SendBlockCond, &m_SendBlockLock);
			}
			else
			{
				uint64_t exptime = CTimer::getTime() + m_iSndTimeOut * 1000ULL;
				timespec locktime;

				locktime.tv_sec = exptime / 1000000;
				locktime.tv_nsec = (exptime % 1000000) * 1000;

				while (!m_bBroken && m_bConnected && !m_bClosing && (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize()) && m_bPeerHealth && (CTimer::getTime() < exptime))
					pthread_cond_timedwait(&m_SendBlockCond, &m_SendBlockLock, &locktime);
			}
			pthread_mutex_unlock(&m_SendBlockLock);
#else
			if (m_iSndTimeOut < 0)
			{
				while (!m_bBroken && m_bConnected && !m_bClosing && (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize()) && m_bPeerHealth)
					WaitForSingleObject(m_SendBlockCond, INFINITE);
			}
			else
			{
				uint64_t exptime = CTimer::getTime() + m_iSndTimeOut * 1000ULL;

				while (!m_bBroken && m_bConnected && !m_bClosing && (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize()) && m_bPeerHealth && (CTimer::getTime() < exptime))
					WaitForSingleObject(m_SendBlockCond, DWORD((exptime - CTimer::getTime()) / 1000));
			}
#endif

			// check the connection status
			if (m_bBroken || m_bClosing)
				throw CUDTException(2, 1, 0);
			else if (!m_bConnected)
				throw CUDTException(2, 2, 0);
			else if (!m_bPeerHealth)
			{
				m_bPeerHealth = true;
				throw CUDTException(7);
			}
		}
	}

	if (!packet_tracker_->CanEnqueuePacket())
	{
		if (m_iSndTimeOut >= 0)
			throw CUDTException(6, 1, 0);

		return 0;
	}

    char* data_ptr = (char*)data;
    int queued = 0;
    while (queued < len) {
        int packet_len = len - queued;
        if (packet_len > m_iPayloadSize) {
            packet_len = m_iPayloadSize;
        }
        CPacket packet;
        packet.m_iSeqNo = GetNextSeqNo();
        packet.setLength(packet_len);
        packet.m_pcData = data_ptr;
        packet_tracker_->EnqueuePacket(packet);
        data_ptr += packet_len;
        queued += packet_len;
    }

	int size = (m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iPayloadSize;
	if (size > len)
		size = len;

	// record total time used for sending
	if (0 == m_pSndBuffer->getCurrBufSize())
		m_llSndDurationCounter = CTimer::getTime();

	// insert the user buffer into the sending list
	m_pSndBuffer->addBuffer(data, size);

	// insert this socket to snd list if it is not on the list yet
	m_pSndQueue->m_pSndUList->update(this, false);

	if (m_iSndBufSize <= m_pSndBuffer->getCurrBufSize())
	{
		// write is not available any more
		s_UDTUnited.m_EPoll.disable_write(m_SocketID, m_sPollID);
	}

	return size;
}

int CUDT::recv(char* data, const int& len)
{
	if (UDT_DGRAM == m_iSockType)
		throw CUDTException(5, 10, 0);

	// throw an exception if not connected
	if (!m_bConnected)
		throw CUDTException(2, 2, 0);
	else if ((m_bBroken || m_bClosing) && (0 == m_pRcvBuffer->getRcvDataSize()))
		throw CUDTException(2, 1, 0);

	if (len <= 0)
		return 0;

	CGuard recvguard(m_RecvLock);

	if (0 == m_pRcvBuffer->getRcvDataSize())
	{
		if (!m_bSynRecving)
			throw CUDTException(6, 2, 0);
		else
		{
#ifndef WIN32
			pthread_mutex_lock(&m_RecvDataLock);
			if (m_iRcvTimeOut < 0)
			{
				while (!m_bBroken && m_bConnected && !m_bClosing && (0 == m_pRcvBuffer->getRcvDataSize()))
					pthread_cond_wait(&m_RecvDataCond, &m_RecvDataLock);
			}
			else
			{
				uint64_t exptime = CTimer::getTime() + m_iRcvTimeOut * 1000ULL;
				timespec locktime;

				locktime.tv_sec = exptime / 1000000;
				locktime.tv_nsec = (exptime % 1000000) * 1000;

				while (!m_bBroken && m_bConnected && !m_bClosing && (0 == m_pRcvBuffer->getRcvDataSize()))
				{
					pthread_cond_timedwait(&m_RecvDataCond, &m_RecvDataLock, &locktime);
					if (CTimer::getTime() >= exptime)
						break;
				}
			}
			pthread_mutex_unlock(&m_RecvDataLock);
#else
			if (m_iRcvTimeOut < 0)
			{
				while (!m_bBroken && m_bConnected && !m_bClosing && (0 == m_pRcvBuffer->getRcvDataSize()))
					WaitForSingleObject(m_RecvDataCond, INFINITE);
			}
			else
			{
				uint64_t enter_time = CTimer::getTime();

				while (!m_bBroken && m_bConnected && !m_bClosing && (0 == m_pRcvBuffer->getRcvDataSize()))
				{
					int diff = int(CTimer::getTime() - enter_time) / 1000;
					if (diff >= m_iRcvTimeOut)
						break;
					WaitForSingleObject(m_RecvDataCond, DWORD(m_iRcvTimeOut - diff ));
				}
			}
#endif
		}
	}

	// throw an exception if not connected
	if (!m_bConnected)
		throw CUDTException(2, 2, 0);
	else if ((m_bBroken || m_bClosing) && (0 == m_pRcvBuffer->getRcvDataSize()))
		throw CUDTException(2, 1, 0);

	int res = m_pRcvBuffer->readBuffer(data, len);

	if (m_pRcvBuffer->getRcvDataSize() <= 0)
	{
		// read is not available any more
		s_UDTUnited.m_EPoll.disable_read(m_SocketID, m_sPollID);
	}

	if ((res <= 0) && (m_iRcvTimeOut >= 0))
		throw CUDTException(6, 2, 0);

	return res;
}

void CUDT::sample(CPerfMon* perf, bool clear)
{
	if (!m_bConnected)
		throw CUDTException(2, 2, 0);
	if (m_bBroken || m_bClosing)
		throw CUDTException(2, 1, 0);

	uint64_t currtime = CTimer::getTime();
	perf->msTimeStamp = (currtime - m_StartTime) / 1000;
        perf->pktTotalBytes = TotalBytes;
	perf->pktSent = m_llTraceSent;
	perf->pktRecv = m_llTraceRecv;
	perf->pktSndLoss = m_iTraceSndLoss;
	perf->pktRcvLoss = m_iTraceRcvLoss;
	perf->pktRetrans = m_iTraceRetrans;
	perf->pktSentACK = m_iSentACK;
	perf->pktRecvACK = m_iRecvACK;
	perf->pktSentNAK = m_iSentNAK;
	perf->pktRecvNAK = m_iRecvNAK;
	perf->usSndDuration = m_llSndDuration;

	perf->pktSentTotal = m_llSentTotal;
	perf->pktRecvTotal = m_llRecvTotal;
	perf->pktSndLossTotal = m_iSndLossTotal;
	perf->pktRcvLossTotal = m_iRcvLossTotal;
	perf->pktRetransTotal = m_iRetransTotal;
	perf->pktSentACKTotal = m_iSentACKTotal;
	perf->pktRecvACKTotal = m_iRecvACKTotal;
	perf->pktSentNAKTotal = m_iSentNAKTotal;
	perf->pktRecvNAKTotal = m_iRecvNAKTotal;
	perf->usSndDurationTotal = m_llSndDurationTotal;

	double interval = double(currtime - m_LastSampleTime);

	perf->mbpsSendRate = double(m_llTraceSent) * m_iPayloadSize * 8.0 / interval;
	perf->mbpsRecvRate = double(m_llTraceRecv) * m_iPayloadSize * 8.0 / interval;

	perf->usPktSndPeriod = GetSendingInterval();
	perf->pktFlowWindow = m_iFlowWindowSize;
	perf->pktCongestionWindow = (int)m_dCongestionWindow;
	perf->pktFlightSize = CSeqNo::seqlen(const_cast<int32_t&>(m_iSndLastAck), CSeqNo::incseq(m_iSndCurrSeqNo)) - 1;
	perf->msRTT = m_iRTT/1000.0;
	perf->mbpsBandwidth = m_iBandwidth * m_iPayloadSize * 8.0 / 1000000.0;

#ifndef WIN32
	if (0 == pthread_mutex_trylock(&m_ConnectionLock))
#else
		if (WAIT_OBJECT_0 == WaitForSingleObject(m_ConnectionLock, 0))
#endif
		{
			perf->byteAvailSndBuf = (NULL == m_pSndBuffer) ? 0 : (m_iSndBufSize - m_pSndBuffer->getCurrBufSize()) * m_iMSS;
			perf->byteAvailRcvBuf = (NULL == m_pRcvBuffer) ? 0 : m_pRcvBuffer->getAvailBufSize() * m_iMSS;

#ifndef WIN32
			pthread_mutex_unlock(&m_ConnectionLock);
#else
			ReleaseMutex(m_ConnectionLock);
#endif
		}
		else
		{
			perf->byteAvailSndBuf = 0;
			perf->byteAvailRcvBuf = 0;
		}

	if (clear)
	{
		m_llTraceSent = m_llTraceRecv = m_iTraceSndLoss = m_iTraceRcvLoss = m_iTraceRetrans = m_iSentACK = m_iRecvACK = m_iSentNAK = m_iRecvNAK = 0;
		m_llSndDuration = 0;
		m_LastSampleTime = currtime;
	}
}

void CUDT::initSynch()
{
#ifndef WIN32
	pthread_mutex_init(&m_SendBlockLock, NULL);
	pthread_cond_init(&m_SendBlockCond, NULL);
	pthread_mutex_init(&m_RecvDataLock, NULL);
	pthread_cond_init(&m_RecvDataCond, NULL);
	pthread_mutex_init(&m_SendLock, NULL);
	pthread_mutex_init(&m_RecvLock, NULL);
	pthread_mutex_init(&m_AckLock, NULL);
	pthread_mutex_init(&m_ConnectionLock, NULL);
	pthread_mutex_init(&m_LossrecordLock, NULL);
#else
	m_SendBlockLock = CreateMutex(NULL, false, NULL);
	m_SendBlockCond = CreateEvent(NULL, false, false, NULL);
	m_RecvDataLock = CreateMutex(NULL, false, NULL);
	m_RecvDataCond = CreateEvent(NULL, false, false, NULL);
	m_SendLock = CreateMutex(NULL, false, NULL);
	m_RecvLock = CreateMutex(NULL, false, NULL);
	m_AckLock = CreateMutex(NULL, false, NULL);
	m_ConnectionLock = CreateMutex(NULL, false, NULL);
#endif
}

void CUDT::destroySynch()
{
#ifndef WIN32
	pthread_mutex_destroy(&m_SendBlockLock);
	pthread_cond_destroy(&m_SendBlockCond);
	pthread_mutex_destroy(&m_RecvDataLock);
	pthread_cond_destroy(&m_RecvDataCond);
	pthread_mutex_destroy(&m_SendLock);
	pthread_mutex_destroy(&m_RecvLock);
	pthread_mutex_destroy(&m_AckLock);
	pthread_mutex_destroy(&m_ConnectionLock);
	pthread_mutex_destroy(&m_LossrecordLock);
#else
	CloseHandle(m_SendBlockLock);
	CloseHandle(m_SendBlockCond);
	CloseHandle(m_RecvDataLock);
	CloseHandle(m_RecvDataCond);
	CloseHandle(m_SendLock);
	CloseHandle(m_RecvLock);
	CloseHandle(m_AckLock);
	CloseHandle(m_ConnectionLock);
#endif
}

void CUDT::releaseSynch()
{
#ifndef WIN32
	// wake up user calls
	pthread_mutex_lock(&m_SendBlockLock);
	pthread_cond_signal(&m_SendBlockCond);
	pthread_mutex_unlock(&m_SendBlockLock);

	pthread_mutex_lock(&m_SendLock);
	pthread_mutex_unlock(&m_SendLock);

	pthread_mutex_lock(&m_RecvDataLock);
	pthread_cond_signal(&m_RecvDataCond);
	pthread_mutex_unlock(&m_RecvDataLock);

	pthread_mutex_lock(&m_RecvLock);
	pthread_mutex_unlock(&m_RecvLock);
#else
	SetEvent(m_SendBlockCond);
	WaitForSingleObject(m_SendLock, INFINITE);
	ReleaseMutex(m_SendLock);
	SetEvent(m_RecvDataCond);
	WaitForSingleObject(m_RecvLock, INFINITE);
	ReleaseMutex(m_RecvLock);
#endif
}

void CUDT::SendAck(int32_t seq_no, int32_t msg_no) {
    CPacket ctrlpkt;
    ctrlpkt.pack(2, NULL, &seq_no, 4);
    ctrlpkt.m_iID = m_PeerID;
    ctrlpkt.m_iMsgNo = msg_no;
    m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);
}

void CUDT::sendCtrl(const int& pkttype, void* lparam, void* rparam, const int& size)
{
	CPacket ctrlpkt;

	switch (pkttype)
	{
	case 2: //010 - Acknowledgement
	{
		assert("Deprecated sendCtrl(2, ...)" && 0);
		break;
	}

	case 6: //110 - Acknowledgement of Acknowledgement
		ctrlpkt.pack(pkttype, lparam);
		ctrlpkt.m_iID = m_PeerID;
		m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

		break;

	case 3: //011 - Loss Report
	{
        assert("Deprecated sendCtrl(3, ...)" && 0);
		break;
	}

	case 4: //100 - Congestion Warning
		ctrlpkt.pack(pkttype);
		ctrlpkt.m_iID = m_PeerID;
		m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

		CTimer::rdtsc(m_ullLastWarningTime);

		break;

	case 1: //001 - Keep-alive
		ctrlpkt.pack(pkttype);
		ctrlpkt.m_iID = m_PeerID;
		m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

		break;

	case 0: //000 - Handshake
		ctrlpkt.pack(pkttype, NULL, rparam, sizeof(CHandShake));
		ctrlpkt.m_iID = m_PeerID;
		m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

		break;

	case 5: //101 - Shutdown
		ctrlpkt.pack(pkttype);
		ctrlpkt.m_iID = m_PeerID;
		m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

		break;

	case 7: //111 - Msg drop request
		ctrlpkt.pack(pkttype, lparam, rparam, 8);
		ctrlpkt.m_iID = m_PeerID;
		m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

		break;

	case 8: //1000 - acknowledge the peer side a special error
		ctrlpkt.pack(pkttype, lparam);
		ctrlpkt.m_iID = m_PeerID;
		m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

		break;

	case 32767: //0x7FFF - Resevered for future use

        if (NULL != rparam)
         {
            ctrlpkt.pack(pkttype, NULL, rparam, size);
            ctrlpkt.m_iID = m_PeerID;
            m_pSndQueue->sendto(m_pPeerAddr, ctrlpkt);

         }
		break;

	default:
		break;
	}
}

void CUDT::add_to_loss_record(int32_t loss1, int32_t loss2){
//TODO: loss record does not have lock, this might cause problem

    AckedPacketVector acked_packets;
    LostPacketVector lost_packets;
    pcc_sender_lock.lock();
    for (int loss = loss1; loss <= loss2; ++loss) {
        int32_t msg_no = packet_tracker_->GetPacketLastMsgNo(loss);
        PacketId pkt_id = packet_tracker_->GetPacketId(loss, msg_no);
        CongestionEvent loss_event;
        loss_event.packet_number = pkt_id;
        loss_event.bytes_acked = 0;
        loss_event.bytes_lost = packet_tracker_->GetPacketSize(loss);
        loss_event.time = CTimer::getTime();
        lost_packets.push_back(loss_event);
        packet_tracker_->OnPacketLoss(loss, msg_no);
        ++m_iSndLossTotal;
    }
    pcc_sender->OnCongestionEvent(true, 0, CTimer::getTime(), 0, acked_packets, lost_packets);
    pcc_sender_lock.unlock();
		
#ifdef EXPERIMENTAL_FEATURE_CONTINOUS_SEND
	pthread_mutex_lock(&m_LossrecordLock);
	loss_record1.push_back(loss1);
	loss_record2.push_back(loss2);
	pthread_mutex_unlock(&m_LossrecordLock);
#endif
}

void CUDT::ProcessAck(CPacket& ctrlpkt) {
    int32_t seq_no = *(int32_t*)ctrlpkt.m_pcData;
    int32_t msg_no = ctrlpkt.m_iMsgNo;
    
    pcc_sender_lock.lock();
    int32_t latest_msg_no = packet_tracker_->GetPacketLastMsgNo(seq_no);
    PacketId pkt_id = packet_tracker_->GetPacketId(seq_no, msg_no);
    PacketState old_state = packet_tracker_->GetPacketState(seq_no);
    packet_tracker_->OnPacketAck(seq_no, msg_no);
    uint64_t rtt_us = packet_tracker_->GetPacketRtt(seq_no, msg_no);
    m_iRTT = (7.0 * m_iRTT + (double)rtt_us) / 8.0;
    m_iRTTVar = (m_iRTTVar * 7.0 + abs((double)rtt_us - m_iRTT) * 1.0) / 8.0;
    int32_t size = packet_tracker_->GetPacketSize(seq_no);

    if (msg_no != latest_msg_no) {
        pcc_sender_lock.unlock();
        return;
    }

    packet_tracker_->DeletePacketRecord(seq_no);
    if (old_state == PACKET_STATE_LOST) {
        pcc_sender_lock.unlock();
        return;
    }
    
    if (pkt_id == 0) {
        pcc_sender_lock.unlock();
        return;
    }

    AckedPacketVector acked_packets;
    LostPacketVector lost_packets;
    CongestionEvent ack_event;
    ack_event.time = CTimer::getTime();
    ack_event.packet_number = pkt_id;
    ack_event.bytes_acked = size;
    ack_event.bytes_lost = 0;
    acked_packets.push_back(ack_event);
    pcc_sender->OnCongestionEvent(true, 0, CTimer::getTime(), rtt_us, acked_packets, lost_packets);
    pcc_sender_lock.unlock();
    ++m_iRecvACK;
    ++m_iRecvACKTotal;
}

void CUDT::processCtrl(CPacket& ctrlpkt)
{
    // Just heard from the peer, reset the expiration count.
	m_iEXPCount = 1;
	uint64_t currtime;
	CTimer::rdtsc(currtime);
	m_ullLastRspTime = currtime;

	switch (ctrlpkt.getType())
	{
	case 2: //010 - Acknowledgement
	{
        ProcessAck(ctrlpkt);
		break;
	}
	case 6: //110 - Acknowledgement of Acknowledgement
	{
		break;
	}

	case 3: //011 - Loss Report
	{
		break;
	}

	case 4: //100 - Delay Warning
		// One way packet delay is increasing, so decrease the sending rate
		m_iLastDecSeq = m_iSndCurrSeqNo;

		break;

	case 1: //001 - Keep-alive
		// The only purpose of keep-alive packet is to tell that the peer is still alive
		// nothing needs to be done.

		break;

	case 0: //000 - Handshake
	{
		CHandShake req;
		req.deserialize(ctrlpkt.m_pcData, ctrlpkt.getLength());
		if ((req.m_iReqType > 0) || (m_bRendezvous && (req.m_iReqType != -2)))
		{
			// The peer side has not received the handshake message, so it keeps querying
			// resend the handshake packet

			CHandShake initdata;
			initdata.m_iISN = m_iISN;
			initdata.m_iMSS = m_iMSS;
			initdata.m_iFlightFlagSize = m_iFlightFlagSize;
			initdata.m_iReqType = (!m_bRendezvous) ? -1 : -2;
			initdata.m_iID = m_SocketID;

			char* hs = new char [m_iRcvPayloadSize];
			int hs_size = m_iRcvPayloadSize;
			initdata.serialize(hs, hs_size);
			sendCtrl(0, NULL, hs, hs_size);
			delete [] hs;
		}

		break;
	}

	case 5: //101 - Shutdown
		m_bShutdown = true;
		m_bClosing = true;
		m_bBroken = true;
		m_iBrokenCounter = 60;

		// Signal the sender and recver if they are waiting for data.
		releaseSynch();

		CTimer::triggerEvent();

		break;

	case 7: //111 - Msg drop request
		m_pRcvBuffer->dropMsg(ctrlpkt.getMsgSeq());
		m_pRcvLossList->remove(*(int32_t*)ctrlpkt.m_pcData, *(int32_t*)(ctrlpkt.m_pcData + 4));

		// move forward with current recv seq no.
		if ((CSeqNo::seqcmp(*(int32_t*)ctrlpkt.m_pcData, CSeqNo::incseq(m_iRcvCurrSeqNo)) <= 0)
				&& (CSeqNo::seqcmp(*(int32_t*)(ctrlpkt.m_pcData + 4), m_iRcvCurrSeqNo) > 0))
		{
			m_iRcvCurrSeqNo = *(int32_t*)(ctrlpkt.m_pcData + 4);
		}

		break;

	case 8: // 1000 - An error has happened to the peer side
		//int err_type = packet.getAddInfo();

		// currently only this error is signalled from the peer side
		// if recvfile() failes (e.g., due to disk fail), blcoked sendfile/send should return immediately
		// giving the app a chance to fix the issue

		m_bPeerHealth = false;

		break;

	case 32767: //0x7FFF - reserved and user defined messages
	{
		break;
	}
	default:
		break;
	}
}

void CUDT::resizeMSS(int mss) {
	//CGuard sendguard(m_SendLock);
    m_iMSS = mss;
	m_iPktSize = m_iMSS - 28;
	m_iPayloadSize = m_iPktSize - CPacket::m_iPktHdrSize;
    m_pSndBuffer->resizeMSS(m_iPayloadSize);
}

uint64_t CUDT::GetSendingInterval() {
    /* Number of clock cyles to wait = (cycles / packet) =
     *
     * (cycles / second) * (bits / packet) * (1 / (bits / second))
     * frequency         * m_iMSS * 8      *  1 / sending_rate (in bits/second)
     */

#ifdef DEBUG_CORE_SENDING_RATE
    static double prev_rate = 0;
    if (pcc_sender->PacingRate(0) != prev_rate) {
        std::cerr << "Sending rate changed to " << pcc_sender->PacingRate(0) / 1000000.0f << "mbps" << std::endl;
        std::cerr << "New clock cycle interval is " << 
            m_ullCPUFrequency * m_iMSS * 8.0f * 1000000.0f / pcc_sender->PacingRate(0)
            << std::endl;
        prev_rate = pcc_sender->PacingRate(0);
    }
#endif
    return m_ullCPUFrequency * m_iMSS * 8.0f * 1000000.0f / pcc_sender->PacingRate(0);
}

int CUDT::packData(CPacket& packet, uint64_t& ts)
{
    int payload = 0;
	uint64_t entertime;
	CTimer::rdtsc(entertime);

    if (m_ullTargetTime != 0) {
        m_ullTimeDiff += (int64_t)entertime - m_ullTargetTime;
    }

    pcc_sender_lock.lock();
    int32_t seq_no;
    if (packet_tracker_->HasRetransmittablePackets()) {
        seq_no = packet_tracker_->GetLowestRetransmittableSeqNo();
        ++m_iTraceRetrans;
		++m_iRetransTotal;
    } else if (packet_tracker_->HasSendablePackets()) {
        seq_no = packet_tracker_->GetLowestSendableSeqNo();
    } else {
        std::cout << "no transmittable packets" << std::endl;
        pcc_sender_lock.unlock();
        return 0;
    }
    char* payload_pointer = packet_tracker_->GetPacketPayloadPointer(seq_no);
    payload = packet_tracker_->GetPacketSize(seq_no);

    packet.m_iSeqNo = seq_no;
    packet.m_pcData = payload_pointer;
    int32_t msg_no = packet_tracker_->GetPacketLastMsgNo(seq_no);
    packet.m_iMsgNo = msg_no + 1;
    packet_tracker_->OnPacketSent(packet);
    PacketId pkt_id = packet_tracker_->GetPacketId(seq_no, packet.m_iMsgNo);
    pcc_sender->OnPacketSent(CTimer::getTime(), 0, pkt_id, payload, false);
    pcc_sender_lock.unlock();

	packet.m_iTimeStamp = int(CTimer::getTime() - m_StartTime);
	packet.m_iID = m_PeerID;
	packet.setLength(payload);

	++m_llSentTotal;
	++m_llTraceSent;
    
    int64_t interval = GetSendingInterval();
    if (m_ullTimeDiff >= interval) {
        ts = entertime;
        m_ullTimeDiff -= interval;
    } else {
        ts = entertime + interval - m_ullTimeDiff;
        m_ullTimeDiff = 0;
    }
	m_ullTargetTime = ts;
    TotalBytes += payload;
	return payload;
}

int CUDT::processData(CUnit* unit)
{
    CPacket& packet = unit->m_Packet;
    SendAck(packet.m_iSeqNo, packet.m_iMsgNo);
	// Just heard from the peer, reset the expiration count.
	m_iEXPCount = 1;
	uint64_t currtime;
	CTimer::rdtsc(currtime);
	m_ullLastRspTime = currtime;

	m_pCC->onPktReceived(&packet);
	++ m_iPktCount;
	// update time information
	m_pRcvTimeWindow->onPktArrival();

	// check if it is probing packet pair
	if (0 == (packet.m_iSeqNo & 0xF))
		m_pRcvTimeWindow->probe1Arrival();
	else if (1 == (packet.m_iSeqNo & 0xF))
		m_pRcvTimeWindow->probe2Arrival();

	++ m_llTraceRecv;
	++ m_llRecvTotal;

	int32_t offset = CSeqNo::seqoff(m_iRcvLastAck, packet.m_iSeqNo);
	if ((offset < 0) || (offset >= m_pRcvBuffer->getAvailBufSize()))
		return -1;

	if (m_pRcvBuffer->addData(unit, offset) < 0)
		return -1;
	
    return 0;
}

int CUDT::listen(sockaddr* addr, CPacket& packet)
{
	if (m_bClosing)
		return 1002;

	if (packet.getLength() != CHandShake::m_iContentSize)
		return 1004;

	CHandShake hs;
	hs.deserialize(packet.m_pcData, packet.getLength());

	// SYN cookie
	char clienthost[NI_MAXHOST];
	char clientport[NI_MAXSERV];
	getnameinfo(addr, (AF_INET == m_iVersion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6), clienthost, sizeof(clienthost), clientport, sizeof(clientport), NI_NUMERICHOST|NI_NUMERICSERV);
	int64_t timestamp = (CTimer::getTime() - m_StartTime) / 60000000; // secret changes every one minute
	stringstream cookiestr;
	cookiestr << clienthost << ":" << clientport << ":" << timestamp;
	unsigned char cookie[16];
	CMD5::compute(cookiestr.str().c_str(), cookie);

	if (1 == hs.m_iReqType)
	{
		hs.m_iCookie = *(int*)cookie;
		packet.m_iID = hs.m_iID;
		int size = packet.getLength();
		hs.serialize(packet.m_pcData, size);
		m_pSndQueue->sendto(addr, packet);
		return 0;
	}
	else
	{
		if (hs.m_iCookie != *(int*)cookie)
		{
			timestamp --;
			cookiestr << clienthost << ":" << clientport << ":" << timestamp;
			CMD5::compute(cookiestr.str().c_str(), cookie);

			if (hs.m_iCookie != *(int*)cookie)
				return -1;
		}
	}

	int32_t id = hs.m_iID;

	// When a peer side connects in...
	if ((1 == packet.getFlag()) && (0 == packet.getType()))
	{
		if ((hs.m_iVersion != m_iVersion) || (hs.m_iType != m_iSockType))
		{
			// mismatch, reject the request
			hs.m_iReqType = 1002;
			int size = CHandShake::m_iContentSize;
			hs.serialize(packet.m_pcData, size);
			packet.m_iID = id;
			m_pSndQueue->sendto(addr, packet);
		}
		else
		{
			int result = s_UDTUnited.newConnection(m_SocketID, addr, &hs);
			if (result == -1)
				hs.m_iReqType = 1002;

			// send back a response if connection failed or connection already existed
			// new connection response should be sent in connect()
			if (result != 1)
			{
				int size = CHandShake::m_iContentSize;
				hs.serialize(packet.m_pcData, size);
				packet.m_iID = id;
				m_pSndQueue->sendto(addr, packet);
			}
			else
			{
				// a mew connection has been created, enable epoll for write
				s_UDTUnited.m_EPoll.enable_write(m_SocketID, m_sPollID);
			}
		}
	}

	return hs.m_iReqType;
}

void CUDT::checkTimers()
{
    bool above_loss_threshold = true;
    uint64_t loss_thresh_us = 2.0 * m_iRTT + 4 * m_iRTTVar;
    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC, &cur_time);

    while (packet_tracker_->HasSentPackets() && above_loss_threshold) {
        int32_t seq_no = packet_tracker_->GetOldestSentSeqNo();
        if (packet_tracker_->HasSentPackets()) {
            int32_t msg_no = packet_tracker_->GetPacketLastMsgNo(seq_no);
            struct timespec sent_time = packet_tracker_->GetPacketSentTime(seq_no, msg_no);
            uint64_t time_since_sent = (cur_time.tv_nsec - sent_time.tv_nsec) / 1000 + (cur_time.tv_sec - sent_time.tv_sec) * 1000000;
            if (time_since_sent > loss_thresh_us) {
                add_to_loss_record(seq_no, seq_no);
            } else {
                above_loss_threshold = false;
            }
        }
    }

	uint64_t currtime;
	CTimer::rdtsc(currtime);

	if ((currtime > m_ullNextACKTime) || ((m_pCC->m_iACKInterval > 0) && (m_pCC->m_iACKInterval <= m_iPktCount)))
	{
		// ACK timer expired or ACK interval is reached

		CTimer::rdtsc(currtime);
		if (m_pCC->m_iACKPeriod > 0)
			m_ullNextACKTime = currtime + m_pCC->m_iACKPeriod * m_ullCPUFrequency;
		else
			m_ullNextACKTime = currtime + m_ullACKInt;

		m_iPktCount = 0;
		m_iLightACKCount = 1;
	}
	else if (m_iSelfClockInterval * m_iLightACKCount <= m_iPktCount)
	{
		//send a "light" ACK
		//sendCtrl(2, NULL, NULL, 4);
		++ m_iLightACKCount;
	}

	// we are not sending back repeated NAK anymore and rely on the sender's EXP for retransmission
	if ((m_pRcvLossList->getLossLength() > 0) && (currtime > m_ullNextNAKTime))
	{
	   CTimer::rdtsc(currtime);
	   m_ullNextNAKTime = currtime + m_ullNAKInt;
	}

	uint64_t next_exp_time;
    uint64_t exp_int = (m_iEXPCount * (m_iRTT + 4 * m_iRTTVar) + m_iSYNInterval) * m_ullCPUFrequency;
    if (exp_int < m_iEXPCount * m_ullMinExpInt)
        exp_int = m_iEXPCount * m_ullMinExpInt;
    next_exp_time = m_ullLastRspTime + exp_int;

	if (currtime > next_exp_time)
	{
		// Haven't receive any information from the peer, is it dead?!
		// timeout: at least 16 expirations and must be greater than 10 seconds
		if ((m_iEXPCount > 16) && (currtime - m_ullLastRspTime > 5000000 * m_ullCPUFrequency))
		{
			//
			// Connection is broken.
			// UDT does not signal any information about this instead of to stop quietly.
			// Application will detect this when it calls any UDT methods next time.
			//
			m_bClosing = true;
			m_bBroken = true;
			m_iBrokenCounter = 30;

			// update snd U list to remove this socket
			m_pSndQueue->m_pSndUList->update(this);

			releaseSynch();

			// app can call any UDT API to learn the connection_broken error
			s_UDTUnited.m_EPoll.enable_read(m_SocketID, m_sPollID);
			s_UDTUnited.m_EPoll.enable_write(m_SocketID, m_sPollID);

			CTimer::triggerEvent();

			return;
		}

        sendCtrl(1);
		++ m_iEXPCount;
		// Reset last response time since we just sent a heart-beat.
		m_ullLastRspTime = currtime;
	}
}

void CUDT::addEPoll(const int eid)
{
	CGuard::enterCS(s_UDTUnited.m_EPoll.m_EPollLock);
	m_sPollID.insert(eid);
	CGuard::leaveCS(s_UDTUnited.m_EPoll.m_EPollLock);

	if (!m_bConnected || m_bBroken || m_bClosing)
		return;

	if ((UDT_STREAM == m_iSockType) && (m_pRcvBuffer->getRcvDataSize() > 0))
		s_UDTUnited.m_EPoll.enable_read(m_SocketID, m_sPollID);
	else if ((UDT_DGRAM == m_iSockType) && (m_pRcvBuffer->getRcvMsgNum() > 0))
		s_UDTUnited.m_EPoll.enable_read(m_SocketID, m_sPollID);

	if (m_iSndBufSize > m_pSndBuffer->getCurrBufSize())
		s_UDTUnited.m_EPoll.enable_write(m_SocketID, m_sPollID);
}

void CUDT::removeEPoll(const int eid)
{
	CGuard::enterCS(s_UDTUnited.m_EPoll.m_EPollLock);
	m_sPollID.erase(eid);
	CGuard::leaveCS(s_UDTUnited.m_EPoll.m_EPollLock);

	// clear IO events notifications;
	// since this happens after the epoll ID has been removed, they cannot be set again
	s_UDTUnited.m_EPoll.disable_read(m_SocketID, m_sPollID);
	s_UDTUnited.m_EPoll.disable_write(m_SocketID, m_sPollID);
}
