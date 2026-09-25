CXX := g++
CPPSRC := $(wildcard src/*.cpp)
OBJS := $(CPPSRC:.cpp=.o)

CXXFLAGS := -g -O3 -DNDEBUG -std=c++20 -pthread

LDLIBS := -lpqxx

.PHONY: all clean

all: main init_db

main: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

init_db:
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ src/init_db.cxx src/trigram_builder.o $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) main init_db