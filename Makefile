.phony: all main clean

all: main

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
CXX=g++-4.7
else
CXX=g++-4.9
endif

CXX_FLAGS=-I. -std=c++11 -MMD -O3
SRCS := $(wildcard *.cpp)
OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)

-include $(DEPS)

main: $(OBJS)
	$(CXX) $(LD_FLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

clean:
	rm -f *.o *.d main
