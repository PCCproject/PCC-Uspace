# Performance-oriented Congestion Control

This repository houses the Performance-oriented Congestion Control (PCC) project.

PCC is a new transport rate control architecture which uses online learning. By empirically observing what actions lead to high performance, PCC achieves substantially higher performance than legacy TCP congestion controllers that are based on fixed rules.  For more, see our original paper on PCC Allegro in [USENIX NSDI 2015](https://www.usenix.org/conference/nsdi15/technical-sessions/presentation/dong), and the paper on PCC Vivace in [USENIX NSDI 2018](https://www.usenix.org/conference/nsdi18/presentation/dong).

## Implementations

This repository contains files for both the UDT and QUIC implementations of PCC. The files in src/pcc/* can be used to build either a UDT or QUIC version of the code; however, the files must be copied to a full QUIC codebase (either Chromium or a QUIC server) before building PCC-QUIC. The UDT version can be built in this repository as described in the next section.

Note: The QUIC version of PCC may require minor type or convention changes depending on the QUIC implementation it is built with because the Chromium and QUIC server codebases have different rules enforced by their build systems. We are working on additional documentation to walk through building PCC QUIC with each of the QUIC codebases.

## Building

To build PCC for UDT, run the following:

```
cd src
sunifdef -r -UQUIC_PORT -UQUIC_PORT_LOCAL ./pcc/\*

make
```

This will produce two apps (pccclient and pccserver) in the src/app directory.

To test that PCC is functioning, you can run:

```
./app/pccserver recv 9000
(this creates a PCC server that listens on port 9000 and receives data)

./app/pccclient send 127.0.0.1 9000
```

This creates a PCC client that connects to the local host (IP 127.0.0.1) at port 9000, then sends data to the server at that address.

The code in this repository is broken into 3 parts:
1. The application code (located in src/app)
2. The UDT library code (located in src/core)
3. The PCC implementation (located in src/pcc)

The PCC code is split into two main parts:
1. The rate control algorithm (located in the src/pcc/pcc_sender files)
2. The monitor interval and utility calculation algorithms (located in the src/pcc/pcc_monitor_interval_queue files)
