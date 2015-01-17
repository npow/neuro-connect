.phony: all main clean

all: main

UNAME := $(shell uname)
FANN_HOME = ./fann

ifeq ($(UNAME), Linux)
CXX = g++-4.6
else
CXX = g++-4.9
endif

CXX_FLAGS=-I. -I$(FANN_HOME)/include -std=c++0x -MMD -O0
LD_FLAGS = -L$(FANN_HOME)/lib -lfann -lstdc++ -lpthread -lboost_thread-mt

SRCS := $(wildcard *.cpp)
OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)

-include $(DEPS)

main: $(OBJS)
	$(CXX) -o $@ $^ $(LD_FLAGS)

%.o: %.cpp
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

clean:
	rm -f *.o *.d main
