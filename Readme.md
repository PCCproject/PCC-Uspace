This branch includes the PCC Vivace implementation as in our [NSDI 2018 paper](https://www.usenix.org/conference/nsdi18/presentation/dong).

## Building

To build PCC Vivace, simply run the following:

```
cd pcc-gradient
make
```

Then, to run UDT-based packet transmission using PCC Vivace, create a PCC server that listens on a port and receives data:

```
export LD_LIBRARY_PATH=pcc-gradient/receiver/src
cd pcc-gradient/receiver/app
./appserver [port]
```

After that, start the PCC Vivace sender that connects to the IP address hosting the PCC server at the server listening port:

```
export LD_LIBRARY_PATH=pcc-gradient/sender/src
cd pcc-gradient/sender/app
./gradient_descent_pcc_client pcc_server_ip pcc_server_port [latency_based]
```

Note that the parameter latency_based determines whether Vivace-Loss (latency_based = 0) or Vivace-Latency (latency_based = 1) is used. By default, latency_based = 0, and the PCC sender is not latency sensitive.