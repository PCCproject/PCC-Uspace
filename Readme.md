# Performance-oriented Congestion Control

This branch is used to test the QUIC-based PCC implementation
(https://github.com/netarch/PCC_QUIC).

## Preparation

The script (pcc/run_google3_to_udt.sh) automatically modifies the PCC/QUIC code
to make it build-able in this UDP-based implementation. Before running the
script, you need to add:
    1) ${dir_google3}: path to QUIC-based PCC_QUIC folder, and
    2) ${dir_udt}: path to PCC-Uspace/pcc folder under this repo.

## Building

To build PCC for UDT, simple run:

```
make
```

This will produce two apps (pccclient and pccserver) in the src/app directory.

To test that PCC is functioning, create a PCC server that listens on port 9000 and receives data:

```
./app/pccserver recv 9000
```

Then, create a PCC client that connects to the local host (IP 127.0.0.1) at port 9000, then sends data to the server at that address:
```
./app/pccclient send 127.0.0.1 9000
```
