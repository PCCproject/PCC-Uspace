#PCC

This repository houses the Performance Congestion Control project.

To build PCC for UDT, run the following:

cd src
sunifdef -r -UQUIC_PORT -UQUIC_PORT_LOCAL ./pcc/\*
make

This will produce two apps (pccclient and pccserver) in the src/app directory.

To test that PCC is functioning, you can run:

./app/pccserver recv 9000
(this creates a PCC server that listens on port 9000 and receives data)

./app/pccclient send 127.0.0.1 9000
(this create a PCC client that connects to the local host (IP 127.0.0.1) at port 9000, then sends data to the server at that address.


The code in this repository is broken into 3 parts:
1. The application code (located in src/app)
2. The UDT library code (located in src/core)
3. The PCC implementation (located in src/pcc)

The PCC code is split into two main parts:
1. The rate control algorithm (located in the src/pcc/pcc_sender files)
2. The monitor interval and utility calculation algorithms (located in the src/pcc/pcc_monitor_interval_queue files)
