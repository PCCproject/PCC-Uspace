C++ = g++ -g -std=c++11

ifndef os
   os = LINUX
endif

ifndef arch
   arch = IA32
endif

CCFLAGS = -fPIC -Wall -Wextra -D$(os) -finline-functions -O3 -fno-strict-aliasing #-msse3

ifeq ($(arch), IA32)
   CCFLAGS += -DIA32
endif

ifeq ($(arch), POWERPC)
   CCFLAGS += -mcpu=powerpc
endif

ifeq ($(arch), SPARC)
   CCFLAGS += -DSPARC
endif

ifeq ($(arch), IA64)
   CCFLAGS += -DIA64
endif

ifeq ($(arch), AMD64)
   CCFLAGS += -DAMD64
endif

OBJS = pcc_monitor_interval_queue.o pcc_sender.o
DIR = $(shell pwd)

all: $(OBJS)

%.o: %.cpp %.h
	$(C++) $(CCFLAGS) $< -c

clean:
	rm -f *.o
