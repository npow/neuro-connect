.phony: all main clean

all: main

UNAME := $(shell uname)
REDIS_CLIENT = $(HOME)/code/redis-cplusplus-client

ifeq ($(UNAME), Linux)
CXX = g++-4.6
else
CXX = g++-4.9
endif

CXX_FLAGS=-I. -std=c++0x -MMD -O0
LD_FLAGS = -lstdc++ -lpthread -lboost_thread-mt
ifeq ($(USE_REDIS), 1)
LD_FLAGS += -L$(REDIS_CLIENT) -lredisclient
endif

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
