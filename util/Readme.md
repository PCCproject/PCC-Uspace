#Utilities

These utilities should be in the util directory:

##port.sh

This script ports UDT core into QUIC by evaluating the QUIC_PORT and QUIC_PORT_LOCAL defines in the code. 

usage: ./port.sh . /path/to/chrome/src

##send_to_emulab.sh

This script sends the pccserver and pccclient from the src/app directory to a remote emulab experiment. It assumes that you have already set up public key access.

usage: ./util/send_to_emulab.sh emulab_username emulab_experiment

example: ./util/send_to_emulab.sh njay2 njay-pcc-trial-1
