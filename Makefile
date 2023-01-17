TARGET=A22-solution
CFLAGS=-Wall -O2
LDFLAGS=
CC=gcc

all: $(TARGET)

$(TARGET): main.c global_defs.h analysis.o configuration.o direct_fork.o fifo_processes.o mq_processes.o reducers.o utility.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c %.h global_defs.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) *.o
