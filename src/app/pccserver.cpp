#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <iostream>
#include "../core/udt.h"
#include "../core/options.h"

using namespace std;

void* senddata(void*);
void* recvdata(void*);
void* recv_monitor(void* s);
void* send_monitor(void* s);

void prusage() {
    cout << "usage: appserver <send|recv> [server_port]" << endl;
    exit(-1);
}

int main(int argc, char* argv[]) {

    if (strcmp(argv[1], "recv") && strcmp(argv[1], "send"))   {
        prusage();
    }
    Options::Parse(argc, argv);

    bool should_recv = !strcmp(argv[1], "recv");

    UDT::startup();

    addrinfo hints;
    addrinfo* res;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    string service(argv[2]);

    if (0 != getaddrinfo(NULL, service.c_str(), &hints, &res)) {
        cout << "illegal port number or port is busy.\n" << endl;
        return 0;
    }

    UDTSOCKET serv = UDT::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (UDT::ERROR == UDT::bind(serv, res->ai_addr, res->ai_addrlen)) {
        cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }

    freeaddrinfo(res);

    cout << "server is ready at port: " << service << endl;

    if (UDT::ERROR == UDT::listen(serv, 10)) {
        cout << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }

    sockaddr_storage clientaddr;
    int addrlen = sizeof(clientaddr);

    UDTSOCKET udt_socket;

    while (true) {
        if (UDT::INVALID_SOCK == (udt_socket = UDT::accept(serv, (sockaddr*)&clientaddr, &addrlen))) {
            cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
            return 0;
        }

        char clienthost[NI_MAXHOST];
        char clientservice[NI_MAXSERV];
        getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
        cout << "new connection: " << clienthost << ":" << clientservice << endl;

        pthread_t worker_thread;
        if (should_recv) {
            pthread_create(&worker_thread, NULL, recvdata, new UDTSOCKET(udt_socket));
        } else {
            pthread_create(&worker_thread, NULL, senddata, new UDTSOCKET(udt_socket));
        }
        pthread_detach(worker_thread);
   }

    UDT::close(serv);
    UDT::cleanup();

    return 0;
}

void* senddata(void* usocket)
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

      return NULL;
}

void* recvdata(void* usocket)
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

      return NULL;
}

void* recv_monitor(void* s)
{
   UDTSOCKET u = *(UDTSOCKET*)s;
    int i = 0;

   UDT::TRACEINFO perf;

   cout << "Recv Rate(Mb/s)\tRTT(ms)\tPackets Recvd" << endl;

   while (true)
   {
      ++i;
         sleep(1);

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

      return NULL;
}

void* send_monitor(void* s)
{
   UDTSOCKET u = *(UDTSOCKET*)s;
    int i = 0;

   UDT::TRACEINFO perf;

   cout << "Send Rate(Mb/s)\tRTT(ms)\t\tSent\t\tLost" << endl;

   while (true)
   {
      ++i;
         sleep(1);

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

      return NULL;
}
