C++ = g++ -g -std=c++0x

ifndef os
   os = LINUX
endif

ifndef arch
   arch = IA32
endif

CCFLAGS = -Wall -D$(os) -I../core -finline-functions -O3

ifeq ($(arch), IA32)
   CCFLAGS += -DIA32 #-mcpu=pentiumpro -march=pentiumpro -mmmx -msse
endif

ifeq ($(arch), POWERPC)
   CCFLAGS += -mcpu=powerpc
endif

ifeq ($(arch), IA64)
   CCFLAGS += -DIA64
endif

ifeq ($(arch), SPARC)
   CCFLAGS += -DSPARC
endif

LDFLAGS = -L../core -ludt -lstdc++ -lpthread -lm

ifeq ($(os), UNIX)
   LDFLAGS += -lsocket
endif

ifeq ($(os), SUNOS)
   LDFLAGS += -lrt -lsocket
endif

DIR = $(shell pwd)

%.o: %.cpp
	$(C++) $(CCFLAGS) $< -c

pccserver: pccserver.o
	$(C++) $^ -o $@ $(LDFLAGS) -static
pccclient: pccclient.o
	$(C++) $^ -o $@ $(LDFLAGS) -static

APP = pccserver pccclient

all: $(APP)

clean:
	rm -f *.o $(APP)

install:
	export PATH=$(DIR):$$PATH
