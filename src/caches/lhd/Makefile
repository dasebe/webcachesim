all : ./bin/cache

HEADERS=$(wildcard *.hpp)

# CFLAGS = -ggdb3 -std=c++14 -Wall -Werror -fPIC -mcmodel=medium
CFLAGS = -march=native -funroll-loops -ffast-math -O3 -std=c++14 -g -fPIC -Werror -Wall -mcmodel=medium
LDFLAGS = -lconfig++

.PHONY: clean
clean:
	rm obj/*.o bin/*

./obj:
	mkdir -p $@

./obj/%.o : %.cpp $(HEADERS) Makefile ./obj
	g++ $(CFLAGS) -c -o $@ $<

./bin/cache : ./obj/cache.o ./obj/repl.o ./obj/lhd.o
	mkdir -p ./bin
	g++ $(CFLAGS) -o $@ $^ $(LDFLAGS)
