.phony: all main clean libfann

all: main

UNAME := $(shell uname)
FANN_HOME = ./fann

ifeq ($(UNAME), Linux)
CXX = g++
else
CXX = g++-4.9
endif

CXX_FLAGS=-I. -I$(FANN_HOME)/src/include -std=c++0x -MMD -O0 -DNDEBUG
LD_FLAGS = -L$(FANN_HOME)/src -lfann

SRCS := $(wildcard *.cpp)
OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)

-include $(DEPS)

libfann:
	cd $(FANN_HOME) && cmake . && make && cd -

main: $(OBJS)
	$(CXX) -o $@ $^ $(LD_FLAGS)

%.o: %.cpp
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

clean:
	rm -f *.o *.d main
