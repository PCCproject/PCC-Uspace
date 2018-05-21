
SOURCES = $(wildcard ./*.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

CCFLAGS += -fPIC

%.o: %.cpp
	$(CC) $(CCFLAGS) $< -c

all: $(OBJECTS)

clean:
	rm -f *.o
