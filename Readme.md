# Performance-oriented Congestion Control

This repository houses the userspace implementations of the Performance-oriented Congestion Control (PCC) project.

PCC is a new transport rate control architecture which uses online learning. By empirically observing what actions lead to high performance, PCC achieves substantially higher performance than legacy TCP congestion controllers that are based on fixed rules.  For more, see our original paper on PCC Allegro in [USENIX NSDI 2015](https://www.usenix.org/conference/nsdi15/technical-sessions/presentation/dong), and the paper on PCC Vivace in [USENIX NSDI 2018](https://www.usenix.org/conference/nsdi18/presentation/dong).

The current PCC implementation employs the Proteus architecture that supports different priorities with different utility functions, e.g., primary, scavenger, hybrid. Details can be found in our recent paper on PCC Proteus in [ACM SIGCOMM 2020](https://dl.acm.org/doi/pdf/10.1145/3387514.3405891)([talk video](https://dl.acm.org/doi/abs/10.1145/3387514.3405891#sec-supp)).

(You can find the version of PCC Vivace as in our NSDI 2018 paper in branch [NSDI-2018](https://github.com/PCCproject/PCC-Uspace/tree/NSDI-2018).)

## Implementations

Our implementation in folder src/ branches off the open-source UDT framework. All PCC related files locate in /src/pcc. Specifically, we implement the transport algorithms using QUIC-compatible function APIs, and QUIC data structures (src/pcc/quic_types/). So PCC should be easy for QUIC adoption.

The code in this repository is broken into 3 parts:
1. The application code (located in src/app)
2. The UDT library code (located in src/core)
3. The PCC implementation (located in src/pcc)


## Building

To build PCC for UDT, simply run the following:

```
cd src
make
```

This will produce two apps (pccclient and pccserver) in the src/app directory.

To test that PCC is functioning, create a PCC server that listens on a port and receives data:

```
./app/pccserver recv pcc_server_port
```

Then, create a PCC client that connects to the IP hosting the PCC server at the server listening port, then sends data to the server at that address (the last parameter tells the application to use the Vivace rate control algorithm, details in our PCC Vivace paper in NSDI 2018):
```
./app/pccclient send pcc_server_ip pcc_server_port Vivace
```

This will by default initiate data transfer using the primary priority utility function. To use a different utility function, give the corresponding parameter (utility function name, and if necessary, utility function parameter) when running PCC client:
```
./app/pccclient send pcc_server_ip pcc_server_port Vivace [utility_function] [utility_parameter]
```

For example,
1. To use primary utility function:
```
./app/pccclient send pcc_server_ip pcc_server_port Vivace Vivace
```

2. To use scavenger utility function (0.0015 is the default coefficient for our scavenger utility function):
```
./app/pccclient send pcc_server_ip pcc_server_port Vivace Scavenger 0.0015
```

3. To use hybrid utility function with some switching threshold (in unit of Mbit/sec, see our PCC Proteus paper in SIGCOMM 2020 for details):
```
./app/pccclient send pcc_server_ip pcc_server_port Vivace Hybrid threshold
```
