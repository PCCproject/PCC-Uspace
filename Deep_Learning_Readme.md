# Deep Learning Instructions

Before attempting to run any deep learning code, follow the instructions in
Readme.md to verify that you can build and a version of standard PCC first.

## Using a trained agent

Start a server as usual:
cd src
export LD\_LIBRARY\_PATH=$LD\_LIBRARY\_PATH:`pwd`/core/
./app/pccserver recv 9000

Start the udt side of the environment:
cd src
export LD\_LIBRARY\_PATH=$LD\_LIBRARY\_PATH:`pwd`/core/
./app/pccclient send 127.0.0.1 9000 --pcc-rate-control=python -pyhelper=loaded\_client -pypath=/path/to/pcc-rl/src/udt-plugins/testing/ --history-len=10 --pcc-utility-calc=linear --model-path=/path/to/your/model/

This should begin running the specified agent on the localhost connection. To run on a real world link, run the sender and receiver on different machines and adjust the IP addresses appropriately.

## Online training

Start a server as usual:
cd src
export LD\_LIBRARY\_PATH=$LD\_LIBRARY\_PATH:`pwd`/core/
./app/pccserver recv 9000

Start the gym side of the training environment from the pcc-rl repo:
cd pcc-rl/gym/training/online/
python3 shim\_solver.py

Start the udt side of the training environment (on the same machine as the shim\_solver):
cd src
export LD\_LIBRARY\_PATH=$LD\_LIBRARY\_PATH:`pwd`/core/
./app/pccclient send 127.0.0.1 9000 --pcc-rate-control=python -pyhelper=shim -path=/path/to/pcc-rl/src/udt-plugins/training/ --history-len=10 --pcc-utility-calc=linear

This should begin online training on the localhost connection. To train on a real world link, run the sender and receiver on different machines and adjust the IP addresses appropriately.
