TARGET=A22-solution
CFLAGS=-Wall -O2
LDFLAGS=
CC=gcc

all: $(TARGET)

$(TARGET): main.c global_defs.h analysis.o configuration.o direct_fork.o fifo_processes.o mq_processes.o reducers.o utility.o cpp_reducer.o
	$(CC) $(CFLAGS) $(LDFLAGS) -lstdc++ -o $@ $^

cpp_reducer.o: cpp_reducer.cpp cpp_reducer.h
	g++ -std=c++20 -o $@ -c cpp_reducer.cpp

%.o: %.c %.h global_defs.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) *.o
