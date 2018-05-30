CXX=g++ -std=c++11
RM=rm -f
CPPFLAGS= -g -I/usr/include/python3.5 -I../src/pcc
LDFLAGS= -g -L/usr/lib/python3.5/config-3.5m-x86_64-linux-gnu/ -L../src/core -lpython3.5 -lstdc++ -lpthread -lm -ludt

SRCS=test.cpp channel_simulator.cpp

OBJS=$(subst .cpp,.o,$(SRCS))

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $< -c

all: test sim_test

test: $(OBJS)
	$(CXX) $(CPPFLAGS) -o test $(OBJS) $(LDFLAGS)

sim_test: test
	cp test sim_test

clean:
	$(RM) $(OBJS) test
