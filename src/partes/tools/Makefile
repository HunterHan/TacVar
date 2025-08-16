CC = mpicc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = -lm

# All tool targets
all: run_nsub_mpi.x run_nsub_mpi_with_w.x

run_nsub_mpi.x: run_nsub_mpi.o
	$(CC) run_nsub_mpi.o -o $@ $(LDFLAGS)

run_nsub_mpi_with_w.x: run_nsub_mpi_with_w.o
	$(CC) run_nsub_mpi_with_w.o -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -I.. -c $< -o $@

clean:
	rm -f *.o *.x

.PHONY: all clean
