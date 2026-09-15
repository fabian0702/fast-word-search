CXX := g++
CPPSRC := $(wildcard src/*.cpp)
OBJS := $(CPPSRC:.cpp=.o)

CXXFLAGS := -g -O3 -DNDEBUG -std=c++20 -pthread

.PHONY: all clean

all: main

main: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) main