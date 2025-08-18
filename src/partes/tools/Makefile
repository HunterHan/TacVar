CC = mpicc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = -lm

# All tool targets
all: meas_single.x meas_series_wd.x meas_pair.x

meas_single.x: meas_single.o
	$(CC) meas_single.o -o $@ $(LDFLAGS)

meas_series_wd.x: meas_series_wd.o
	$(CC) meas_series_wd.o -o $@ $(LDFLAGS)

meas_pair.x: meas_pair.o
	$(CC) meas_pair.o -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -I.. -c $< -o $@

clean:
	rm -f *.o *.x

.PHONY: all clean
