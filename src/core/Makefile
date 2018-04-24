
CCFLAGS += -fPIC

SOURCES = $(wildcard ./*.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

all: $(OBJECTS)

%.o: %.cpp
	$(CC) $(CCFLAGS) $< -c

clean:
	rm -f *.o *.so *.dylib *.a
