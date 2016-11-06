CXX=g++
RM=rm -f
CPPFLAGS=-g -std=c++11
LDFLAGS=-g -std=c++11

SRCSIM=webcachesim.cc
OBJS=$(subst .cc,.o,$(SRCSIM))

all: webcachesim

webcachesim: $(OBJS)
	$(CXX) $(LDFLAGS) -o webcachesim $(OBJS)

clean:
	$(RM) $(OBJS)

dist-clean: clean
	$(RM) tool
