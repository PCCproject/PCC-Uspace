CXX=g++ -std=c++11
RM=rm -f
CPPFLAGS= -g -I/usr/include/python3.5
LDFLAGS= -g -L/usr/lib/python3.5/config-3.5m-x86_64-linux-gnu/ -lpython3.5 -lstdc++ -lpthread -lm

SRCS=test.cpp ../src/core/md5.cpp ../src/core/common.cpp ../src/core/options.cpp ../src/pcc/pcc_logger.cpp ../src/pcc/pcc_monitor_interval_queue.cpp ../src/pcc/pcc_sender.cpp ../src/pcc/pcc_python_helper.cpp channel_simulator.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

all: test

test: $(OBJS)
	$(CXX) $(CPPFLAGS) -o test $(OBJS) $(LDFLAGS)

test.o: test.cpp channel_simulator.h

channel_simulator.o: channel_simulator.cpp channel_simulator.h

logging.o: logging.cpp logging.h

sender.o: sender.cpp sender.h

pcc_monitor_interval_queue.o: pcc_monitor_interval_queue.cpp pcc_monitor_interval_queue.h

pcc_utilities.o: pcc_utilities.cpp pcc_utilities.h

allegro_sender.o: allegro_sender.cpp allegro_sender.h

vivace_sender.o: vivace_sender.cpp vivace_sender.h

rinforzando.o: rinforzando_sender.cpp rinforzando_sender.h

clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) test
