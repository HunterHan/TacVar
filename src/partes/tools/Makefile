CC = mpicc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = -lm

# All tool targets
all: meas_single.x meas_single_with_w.x meas_pair.x

meas_single.x: meas_single.o
	$(CC) meas_single.o -o $@ $(LDFLAGS)

meas_single_with_w.x: meas_single_with_w.o
	$(CC) meas_single_with_w.o -o $@ $(LDFLAGS)

meas_pair.x: meas_pair.o
	$(CC) meas_pair.o -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -I.. -c $< -o $@

clean:
	rm -f *.o *.x

.PHONY: all clean
