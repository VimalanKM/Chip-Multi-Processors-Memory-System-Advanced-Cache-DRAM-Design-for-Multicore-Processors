SRCS = cache.cpp core.cpp dram.cpp memsys.cpp sim.cpp
OBJS = $(SRCS:.cpp=.o)

CXX = g++
CXXFLAGS = -g -Wall -Werror -pedantic -std=c++11
TARBALL = ../lab4.tar.gz

.PHONY: all sim clean profile debug validate runall fast submit

all: sim

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

sim: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean: 
	-rm -f sim $(OBJS)

profile: CXXFLAGS += -O2 -pg
profile: all

debug: CXXFLAGS += -DDEBUG
debug: all

validate: all
validate: 
	@bash ../scripts/runtests.sh

runall: all
runall:
	@bash ../scripts/runall.sh

fast: CXXFLAGS += -O2
fast: all

submit:
	tar -czvf $(TARBALL) -C .. src
	@echo 'Created! Please check the tarball to ensure it was made correctly!'
	@echo 'You are solely responsible for what you submit!'