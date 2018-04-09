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
#include <signal.h>

using namespace std;

#ifndef WIN32
void* monitor(void*);
#else
DWORD WINAPI monitor(LPVOID);
#endif

void intHandler(int dummy) {
	//TODO (nathan jay): Print useful summary statistics.
    exit(0);
}


int main(int argc, char* argv[]) {
    
    if ((argc < 4) || (0 == atoi(argv[3]))) {
        cout << "usage: " << argv[0] << " <send|recv> server_ip server_port" << endl;
      return 0;
    }

    bool should_send = !strcmp(argv[1], "send");
    
	signal(SIGINT, intHandler);
   
    // use this function to initialize the UDT library
    UDT::startup();

    struct addrinfo hints, *local, *peer;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (0 != getaddrinfo(NULL, "9000", &hints, &local)) {
        cout << "incorrect network address.\n" << endl;
        return 0;
    }

    UDTSOCKET client = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);

    // Windows UDP issue
    // For better performance, modify HKLM\System\CurrentControlSet\Services\Afd\Parameters\FastSendDatagramThreshold
    #ifdef WIN32
        UDT::setsockopt(client, 0, UDT_MSS, new int(1052), sizeof(int));
     #endif

   freeaddrinfo(local);

   if (0 != getaddrinfo(argv[2], argv[3], &hints, &peer))
   {
      cout << "incorrect server/peer address. " << argv[1] << ":" << argv[2] << endl;
      return 0;
   }

   // connect to the server, implict bind
   if (UDT::ERROR == UDT::connect(client, peer->ai_addr, peer->ai_addrlen))
   {
      cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }
   freeaddrinfo(peer);

   // using CC method
   int temp;

   int size = 100000;
   char* data = new char[size];
   bzero(data, size);

   #ifndef WIN32
      pthread_create(new pthread_t, NULL, monitor, &client);
   #else
      CreateThread(NULL, 0, monitor, &client, 0, NULL);
   #endif

    if (should_send) {
        while (true) {
            int ssize = 0;
            int ss;
            while (ssize < size) {
                if (UDT::ERROR == (ss = UDT::send(client, data + ssize, size - ssize, 0))) {
                    cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
                    break;
                }

                ssize += ss;
            }

            if (ssize < size)
                break;
        }
    } else {
        while (true) {
            int rsize = 0;
            int rs;
            while (rsize < size) {
                if (UDT::ERROR == (rs = UDT::recv(client, data + rsize, size - rsize, 0))) {
                    cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
                    break;
                }

                rsize += rs;
            }

            if (rsize < size)
                break;
        }
    }

   UDT::close(client);

   delete [] data;

   // use this function to release the UDT library
   UDT::cleanup();

   return 1;
}

#ifndef WIN32
void* monitor(void* s)
#else
DWORD WINAPI monitor(LPVOID s)
#endif
{
   UDTSOCKET u = *(UDTSOCKET*)s;

   UDT::TRACEINFO perf;

   cout << "SendRate(Mb/s)\tRTT(ms)\tCTotal\tLoss\tRecvACK\tRecvNAK" << endl;
   int i=0;
   while (true)
   {
      #ifndef WIN32
         usleep(1000000);
      #else
         Sleep(1000);
      #endif
        i++;
      if (UDT::ERROR == UDT::perfmon(u, &perf))
      {
         cout << "perfmon: " << UDT::getlasterror().getErrorMessage() << endl;
         break;
      }
    cout   <<""<<i<<"\t" << perf.mbpsSendRate << "\t"
           << perf.msRTT << "\t"
           <<  perf.pktSentTotal << "\t"
           << perf.pktSndLossTotal <<endl;
   }

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}

