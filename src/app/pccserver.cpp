#ifndef WIN32
   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif
#include <iostream>
#include "../core/udt.h"
#include "test_util.h"

using namespace std;

#ifndef WIN32
void* senddata(void*);
void* recvdata(void*);
void* recv_monitor(void* s);
void* send_monitor(void* s);
#else
DWORD WINAPI recvdata(LPVOID);
#endif

int main(int argc, char* argv[])
{
   if ((strcmp(argv[1], "recv") && strcmp(argv[1], "send")) || ((2 != argc) && ((3 != argc) || (0 == atoi(argv[2])))))
   {
      cout << "usage: appserver <send|recv> [server_port]" << endl;
      return 0;
   }

   bool should_recv = !strcmp(argv[1], "recv");

   // Automatically start up and clean up UDT module.
   UDTUpDown _udt_;

   addrinfo hints;
   addrinfo* res;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   //hints.ai_socktype = SOCK_DGRAM;

   string service("9000");
   if (argc == 3)
      service = argv[2];

   if (0 != getaddrinfo(NULL, service.c_str(), &hints, &res))
   {
      cout << "illegal port number or port is busy.\n" << endl;
      return 0;
   }

   UDTSOCKET serv = UDT::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

   // UDT Options
   //UDT::setsockopt(serv, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
   //UDT::setsockopt(serv, 0, UDT_MSS, new int(9000), sizeof(int));
   //UDT::setsockopt(serv, 0, UDT_RCVBUF, new int(10000000), sizeof(int));
   //UDT::setsockopt(serv, 0, UDP_RCVBUF, new int(10000000), sizeof(int));

   if (UDT::ERROR == UDT::bind(serv, res->ai_addr, res->ai_addrlen))
   {
      cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   freeaddrinfo(res);

   cout << "server is ready at port: " << service << endl;

   if (UDT::ERROR == UDT::listen(serv, 10))
   {
      cout << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   sockaddr_storage clientaddr;
   int addrlen = sizeof(clientaddr);

   UDTSOCKET udt_socket;

   while (true)
   {
      if (UDT::INVALID_SOCK == (udt_socket = UDT::accept(serv, (sockaddr*)&clientaddr, &addrlen)))
      {
         cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
         return 0;
      }

      char clienthost[NI_MAXHOST];
      char clientservice[NI_MAXSERV];
      getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
      cout << "new connection: " << clienthost << ":" << clientservice << endl;

      #ifndef WIN32
         pthread_t worker_thread;
         if (should_recv) {
             pthread_create(&worker_thread, NULL, recvdata, new UDTSOCKET(udt_socket));
         } else {
             pthread_create(&worker_thread, NULL, senddata, new UDTSOCKET(udt_socket));
         }
         pthread_detach(worker_thread);
      #else
         if (should_recv) {
            CreateThread(NULL, 0, recvdata, new UDTSOCKET(udt_socket), 0, NULL);
         } else {
            CreateThread(NULL, 0, senddata, new UDTSOCKET(udt_socket), 0, NULL);
         }
      #endif
   }

   UDT::close(serv);

   return 0;
}

#ifndef WIN32
void* senddata(void* usocket)
#else
DWORD WINAPI senddata(LPVOID usocket)
#endif
{
   UDTSOCKET sender = *(UDTSOCKET*)usocket;
   delete (UDTSOCKET*)usocket;
   pthread_create(new pthread_t, NULL, send_monitor, &sender);
   char* data;
   int size = 100000000;
   data = new char[size];

   while (true)
   {
      int ssize = 0;
      int ss;
      while (ssize < size)
      {
         if (UDT::ERROR == (ss = UDT::send(sender, data + ssize, size - ssize, 0)))
         {
            cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
         }
         ssize += ss;
      }

      if (ssize < size)
         break;
   }

   delete [] data;

   UDT::close(sender);

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}

#ifndef WIN32
void* recvdata(void* usocket)
#else
DWORD WINAPI recvdata(LPVOID usocket)
#endif
{
   UDTSOCKET recver = *(UDTSOCKET*)usocket;
   delete (UDTSOCKET*)usocket;
   pthread_create(new pthread_t, NULL, recv_monitor, &recver);
   char* data;
   int size = 100000000;
   data = new char[size];

   while (true)
   {
      int rsize = 0;
      int rs;
      while (rsize < size)
      {
         int rcv_size;
         int var_size = sizeof(int);
         //UDT::getsockopt(recver, 0, UDT_RCVDATA, &rcv_size, &var_size);
         if (UDT::ERROR == (rs = UDT::recv(recver, data + rsize, size - rsize, 0)))
         {
            cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
         }

         rsize += rs;
      }

      if (rsize < size)
         break;
   }

   delete [] data;

   UDT::close(recver);

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}

#ifndef WIN32
void* recv_monitor(void* s)
#else
DWORD WINAPI recv_monitor(LPVOID s)
#endif
{
   UDTSOCKET u = *(UDTSOCKET*)s;
    int i = 0;

   UDT::TRACEINFO perf;

   cout << "Recv Rate(Mb/s)\tRTT(ms)\tPackets Recvd" << endl;

   while (true)
   {
      ++i;
      #ifndef WIN32
         sleep(1);
      #else
         Sleep(1000);
      #endif

      if (UDT::ERROR == UDT::perfmon(u, &perf))
      {
         cout << "perfmon: " << UDT::getlasterror().getErrorMessage() << endl;
         break;
      }

      cout << perf.mbpsRecvRate << "\t\t"
           << perf.msRTT << "\t"
           << perf.pktRecv << "\t\t"
           << std::endl;
   }

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}

#ifndef WIN32
void* send_monitor(void* s)
#else
DWORD WINAPI send_monitor(LPVOID s)
#endif
{
   UDTSOCKET u = *(UDTSOCKET*)s;
    int i = 0;

   UDT::TRACEINFO perf;

   cout << "Send Rate(Mb/s)\tRTT(ms)\t\tSent\t\tLost" << endl;

   while (true)
   {
      ++i;
      #ifndef WIN32
         sleep(1);
      #else
         Sleep(1000);
      #endif

      if (UDT::ERROR == UDT::perfmon(u, &perf))
      {
         cout << "perfmon: " << UDT::getlasterror().getErrorMessage() << endl;
         break;
      }

      cout << perf.mbpsSendRate << "\t\t"
           << perf.msRTT << "\t"
           << perf.pktSentTotal << "\t"
           << perf.pktSndLossTotal << "\t"
           << std::endl;
   }

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}
