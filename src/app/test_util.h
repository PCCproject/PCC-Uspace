#ifndef _UDT_TEST_UTILS_H_
#define _UDT_TEST_UTILS_H_

#include <arpa/inet.h>
#include <iostream>
#include <linux/netfilter_ipv4.h>
#include <sstream>
#include <string>

#ifndef WIN32
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <unistd.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#endif

using namespace std;

struct UDTUpDown {
  // use this function to initialize the UDT library
  UDTUpDown() {UDT::startup();}
  // use this function to release the UDT library
  ~UDTUpDown() {UDT::cleanup();}
};

struct proxy_socket_pair {
  UDTSOCKET udt_socket;
  int tcp_socket;
};

UDTSOCKET MakeUdtConnection(int argc, char* argv[]) {
  struct addrinfo hints, *local, *peer;

  memset(&hints, 0, sizeof(struct addrinfo));

  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (0 != getaddrinfo(NULL, "9000", &hints, &local)) {
    cerr << "incorrect network address.\n" << endl;
    return 0;
  }

  // CUDT class constructor will be called here.
  UDTSOCKET udt_socket =
      UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);
  freeaddrinfo(local);

  // Set PCC control algorithm.
  string pcc_control_algorithm = "Allegro";
  if (argc >= 5) {
    pcc_control_algorithm = argv[4];
  }
  UDT::setsockopt(udt_socket, 0, UDT_PCC, &pcc_control_algorithm,
                  pcc_control_algorithm.size());
  // Set PCC utility tag and parameters.
  string utility_tag = "Allegro";
  if (argc >= 6) {
    utility_tag = argv[5];
  }
  UDT::setsockopt(udt_socket, 0, UDT_UTAG, &utility_tag, utility_tag.size());
  if (utility_tag == "HybridAllegro" || utility_tag == "HybridVivace") {
    UDT::setsockopt(udt_socket, 0, UDT_UPARAM, new float(atof(argv[6])),
                    sizeof(float));
  }

#ifdef WIN32
  // Windows UDP issue
  // For better performance, modify HKLM\System\CurrentControlSet\Services\Afd\
  // \Parameters\FastSendDatagramThreshold
  UDT::setsockopt(udt_socket, 0, UDT_MSS, new int(1052), sizeof(int));
#endif

  if (0 != getaddrinfo(argv[1], argv[2], &hints, &peer)) {
    cerr << "incorrect server/peer address. " << argv[1] << ":" << argv[2]
         << endl;
    return 0;
  }

  // connect to the server, implict bind
  if (UDT::ERROR == UDT::connect(udt_socket, peer->ai_addr, peer->ai_addrlen)) {
    cerr << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
    return 0;
  }
  freeaddrinfo(peer);

  return udt_socket;
}

UDTSOCKET CreateUdtServerSocket(string port) {
  addrinfo hints, *local;
  memset(&hints, 0, sizeof(struct addrinfo));

  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (0 != getaddrinfo(NULL, port.c_str(), &hints, &local)) {
    cerr << "illegal port number or port is busy.\n" << endl;
    return 0;
  }

  UDTSOCKET udt_socket =
      UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);

  if (UDT::ERROR == UDT::bind(udt_socket, local->ai_addr, local->ai_addrlen)) {
    cerr << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
    return 0;
  }

  freeaddrinfo(local);
  cout << "server is ready at port: " << port << endl;
  return udt_socket;
}

int CreateTcpListenerSocket(string port) {
  int status;
  struct addrinfo hints, *local;
  memset(&hints, 0, sizeof hints);

  // IP version not specified. Can be both.
  hints.ai_family = AF_UNSPEC;
  // Use SOCK_STREAM for TCP or SOCK_DGRAM for UDP.
  hints.ai_socktype = SOCK_STREAM;
  // IP Wildcard
  hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(NULL, port.c_str(), &hints, &local)) != 0) {
    cerr << "getaddrinfo error: " << gai_strerror(status);
  }
  // Create socket that listens outgoing TCP traffic.
  int sock = socket(local->ai_family, local->ai_socktype, local->ai_protocol);
  if (sock == -1) {
    cerr << "socket error" << endl;
    exit(-1);
  }
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, new int(1), sizeof(int));
  if (::bind(sock, local->ai_addr, local->ai_addrlen) == -1) {
    cerr << "bind error" << endl;
    exit(-1);
  }

  return sock;
}

int CreateProxyTcpSocket(int port) {
  int status;
  struct addrinfo hints, *peer;
  memset(&hints, 0, sizeof hints);

  // IP version not specified. Can be both.
  hints.ai_family = AF_UNSPEC;
  // Use SOCK_STREAM for TCP or SOCK_DGRAM for UDP.
  hints.ai_socktype = SOCK_STREAM;

  // Now fill up the linked list of hints structs with google's address information.
  stringstream port_strs;
  port_strs << port;
  status = getaddrinfo("127.0.0.1", port_strs.str().c_str(), &hints, &peer);
  if (status != 0) {
    cout << "getaddrinfo error: " << gai_strerror(status) ;
  }

  cout << "Connecting to port " << port << endl;
  int sock = socket(peer->ai_family, peer->ai_socktype, peer->ai_protocol);
  if (sock == -1) {
    cout << "socket error ";
    exit(-1);
  }

  status = connect(sock, peer->ai_addr, peer->ai_addrlen);
  if (status == -1){
    cout << "connect error" ;
    exit(-1);
  }
  cout << "Connected!"  << endl;

  return sock;
}

void SendProxiedConnectionMetadata(UDTSOCKET udt_socket, int dst_port) {
  int write_size = -1;
  int network_id = htonl(dst_port);
  write_size = UDT::send(udt_socket, (const char*)&network_id, sizeof(int), 0);
  if (write_size == UDT::ERROR) {
    cerr << "Send Destination Port Error: "
         << UDT::getlasterror().getErrorMessage() << endl;
  }
}

void* ProxyUdtListener(void* arguments) {
  struct proxy_socket_pair socket_pair = *(struct proxy_socket_pair*)arguments;
  int tcp_socket = socket_pair.tcp_socket;
  UDTSOCKET udt_socket = socket_pair.udt_socket;

  int size = 500000;
  char* buffer = new char[size];
  int read_size = 0, write_size = 0;
  while (true) {
    if (UDT::ERROR == (read_size = UDT::recv(udt_socket, buffer, size, 0))) {
      cerr << "Error receiving from UDT: "
           << UDT::getlasterror().getErrorMessage() << endl;
      break;
    }
    if (-1 == (write_size = send(tcp_socket, buffer, read_size, 0))) {
      cerr << "Error writing to TCP" << endl;
      break;
    }
    if (write_size < read_size) {
      cerr << "Error TCP write less than UDT read: " << write_size << " vs "
           << read_size << endl;
      break;
    }
  }

  delete [] buffer;
  close(tcp_socket);
  UDT::close(udt_socket);
  return NULL;
}

void* ProxyTcpListener(void* arguments) {
  struct proxy_socket_pair socket_pair = *(struct proxy_socket_pair*)arguments;
  int tcp_socket = socket_pair.tcp_socket;
  UDTSOCKET udt_socket = socket_pair.udt_socket;

  int size = 500000;
  char* buffer = new char[size];
  int read_size = 0, write_size = 0, write_size_single = 0;
  while (true) {
    if (-1 == (read_size = recv(tcp_socket, buffer, size, 0))) {
      cerr << "Error receiving from TCP: "
           << UDT::getlasterror().getErrorMessage() << endl;
      break;
    }
    write_size = 0;
    while (write_size < read_size) {
      if (UDT::ERROR ==
          (write_size_single = UDT::send(udt_socket, buffer + write_size,
                                         read_size - write_size, 0))) {
        cerr << "Error writing to UDT" << endl;
        break;
      }
      write_size += write_size_single;
    }
    /*if (write_size < read_size) {
      cerr << "Error UDT write less than TCP read: " << write_size << " vs "
           << read_size << endl;
      break;
    }*/
  }

  delete [] buffer;
  close(tcp_socket);
  UDT::close(udt_socket);
  return NULL;
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
  while (true) {
#ifndef WIN32
    usleep(1000000);
#else
    Sleep(1000);
#endif
    i++;
    if (UDT::ERROR == UDT::perfmon(u, &perf)) {
      cout << "perfmon: " << UDT::getlasterror().getErrorMessage() << endl;
      break;
    }
    cout << i << "\t" << perf.mbpsSendRate << "\t" << perf.msRTT << "\t"
         << perf.pktSentTotal << "\t" << perf.pktSndLossTotal << endl;
  }

#ifndef WIN32
  return NULL;
#else
  return 0;
#endif
}

void ListenForOutgoingTcpConnection(int argc, char* argv[]) {
  int sock = CreateTcpListenerSocket(argv[3]);

  if (listen(sock, 25) == -1) {
    cerr << "listen error" << endl;
    return;
  }
  cout << "Listening for outgoing TCP connections at port " << argv[3] << endl;

  sockaddr_storage peer;
  int addr_len = sizeof(peer);
  socklen_t addr_size = sizeof(peer);
  char peer_host[NI_MAXHOST];
  char peer_port[NI_MAXSERV];
  while (true) {
    // Waiting for outgoing TCP request.
    int tcp_socket = accept(sock, (struct sockaddr *)&peer, &addr_size);
    if (tcp_socket < 0) {
      exit (-1);
    }

    getnameinfo((sockaddr *)&peer, addr_len, peer_host, sizeof(peer_host),
                peer_port, sizeof(peer_port), NI_NUMERICHOST|NI_NUMERICSERV);
    cout << "new TCP connection: " << peer_host << ":" << peer_port << endl;

    struct sockaddr_in addr;
    bzero((char *)&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr_size = sizeof(addr);
    getsockopt(tcp_socket, SOL_IP, SO_ORIGINAL_DST, &addr, &addr_size);
    char orig_dst_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, orig_dst_str, INET_ADDRSTRLEN);
    string hostname = string(orig_dst_str);
    int port = ntohs(addr.sin_port);
    cout << "Destination: " << hostname << ":" << port << endl;

    // Create a (proxy) UDT socket to pair with the outgoing TCP connection.
    UDTSOCKET udt_socket = MakeUdtConnection(argc, argv);

    SendProxiedConnectionMetadata(udt_socket, port);
    struct proxy_socket_pair socket_pair;
    socket_pair.udt_socket = udt_socket;
    socket_pair.tcp_socket = tcp_socket;
    pthread_t thread_id;
    pthread_create(new pthread_t, NULL, monitor, &udt_socket);
    pthread_create(&thread_id, NULL, ProxyUdtListener, (void*)&socket_pair);
    pthread_create(&thread_id, NULL, ProxyTcpListener, (void*)&socket_pair);
    pthread_join(thread_id, NULL);
  }

  close(sock);
}

void ListenForIncomingUdtConnection(string port) {
  UDTSOCKET udt_server_socket = CreateUdtServerSocket(port);

  if (UDT::ERROR == UDT::listen(udt_server_socket, 10)) {
    cerr << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
    return;
  }

  sockaddr_storage peer;
  int addr_len = sizeof(peer);
  UDTSOCKET udt_socket;
  char peer_host[NI_MAXHOST];
  char peer_port[NI_MAXSERV];
  while (true) {
    udt_socket =
        UDT::accept(udt_server_socket, (sockaddr*)&peer, &addr_len);
    if (UDT::INVALID_SOCK == udt_socket) {
      cerr << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
      exit(-1);
    }

    getnameinfo((sockaddr *)&peer, addr_len, peer_host, sizeof(peer_host),
                peer_port, sizeof(peer_port), NI_NUMERICHOST|NI_NUMERICSERV);
    cout << "new connection: " << peer_host << ":" << peer_port << endl;

    int network_id;
    if (UDT::ERROR ==
        UDT::recv(udt_socket, (char*)&network_id, sizeof(int), 0)) {
      cerr << "Recv Destination Port Error: "
           << UDT::getlasterror().getErrorMessage() << endl;
      break;
    }
    int dst_port = ntohl(network_id);
    cout << "new TCP connection to port " << dst_port << endl;

    int tcp_socket = CreateProxyTcpSocket(dst_port);
    struct proxy_socket_pair socket_pair;
    socket_pair.udt_socket = udt_socket;
    socket_pair.tcp_socket = tcp_socket;
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, ProxyUdtListener, (void*)&socket_pair);
    pthread_create(&thread_id, NULL, ProxyTcpListener, (void*)&socket_pair);
    pthread_join(thread_id, NULL);
  }

  UDT::close(udt_server_socket);
}

void ListenForIncomingTcpConnection(int argc, char* argv[]) {
  while (true) {
    // Create a udt socket in preparation for the next TCP request from passive
    // pcc server.
    UDTSOCKET udt_socket = MakeUdtConnection(argc, argv);
    int network_id;
    if (UDT::ERROR ==
        UDT::recv(udt_socket, (char*)&network_id, sizeof(int), 0)) {
      cerr << "Recv Destination Port Error: "
           << UDT::getlasterror().getErrorMessage() << endl;
      break;
    }
    int dst_port = ntohl(network_id);
    cout << "new TCP request to port " << dst_port << endl;

    int tcp_socket = CreateProxyTcpSocket(dst_port);
    struct proxy_socket_pair socket_pair;
    socket_pair.udt_socket = udt_socket;
    socket_pair.tcp_socket = tcp_socket;
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, ProxyUdtListener, (void*)&socket_pair);
    pthread_create(&thread_id, NULL, ProxyTcpListener, (void*)&socket_pair);
    pthread_join(thread_id, NULL);
  }
}

void ListenForOutgoingTcpRequest(string udt_port, string tcp_proxy_port) {
  UDTSOCKET udt_server_socket = CreateUdtServerSocket(udt_port);
  if (UDT::ERROR == UDT::listen(udt_server_socket, 10)) {
    cerr << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
    return;
  }
  cout << "Listening for incoming UDT connection at port " << udt_port << endl;

  int sock = CreateTcpListenerSocket(tcp_proxy_port);
  if (listen(sock, 25) == -1) {
    cerr << "listen error" << endl;
    return;
  }
  cout << "Listening for outgoing TCP requests at port " << tcp_proxy_port;
  cout << endl;

  sockaddr_storage peer, local;
  int addr_len = sizeof(peer);
  socklen_t addr_size = sizeof(peer);
  UDTSOCKET udt_socket;
  char peer_host[NI_MAXHOST], local_host[NI_MAXHOST];
  char peer_port[NI_MAXSERV], local_port[NI_MAXSERV];
  while (true) {
    udt_socket =
        UDT::accept(udt_server_socket, (sockaddr*)&peer, &addr_len);
    if (UDT::INVALID_SOCK == udt_socket) {
      cerr << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
      exit(-1);
    }
    getnameinfo((sockaddr *)&peer, addr_len, peer_host, sizeof(peer_host),
                peer_port, sizeof(peer_port), NI_NUMERICHOST|NI_NUMERICSERV);
    cout << "new udt connection: " << peer_host << ":" << peer_port << endl;

    // Waiting for outgoing TCP request.
    int tcp_socket = accept(sock, (struct sockaddr *)&local, &addr_size);
    if (tcp_socket < 0) {
      exit (-1);
    }
    getnameinfo((sockaddr *)&local, addr_len, local_host, sizeof(local_host),
                local_port, sizeof(local_port), NI_NUMERICHOST|NI_NUMERICSERV);
    cout << "new TCP connection: " << peer_host << ":" << peer_port << endl;

    struct sockaddr_in addr;
    bzero((char *)&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr_size = sizeof(addr);
    getsockopt(tcp_socket, SOL_IP, SO_ORIGINAL_DST, &addr, &addr_size);
    char orig_dst_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, orig_dst_str, INET_ADDRSTRLEN);
    string hostname = string(orig_dst_str);
    int port = ntohs(addr.sin_port);
    cout << "Destination: " << hostname << ":" << port << endl;

    SendProxiedConnectionMetadata(udt_socket, port);

    struct proxy_socket_pair socket_pair;
    socket_pair.udt_socket = udt_socket;
    socket_pair.tcp_socket = tcp_socket;
    pthread_t thread_id;
    pthread_create(new pthread_t, NULL, monitor, &udt_socket);
    pthread_create(&thread_id, NULL, ProxyUdtListener, (void*)&socket_pair);
    pthread_create(&thread_id, NULL, ProxyTcpListener, (void*)&socket_pair);
    pthread_join(thread_id, NULL);
  }

  UDT::close(udt_server_socket);
  close(sock);
}

#endif
