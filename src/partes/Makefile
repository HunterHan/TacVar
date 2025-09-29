CC = mpicc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = -lm
MPIFLAGS = -DPTOPT_USE_MPI

# Source directories
KERNELDIR = kernels
GAUGEDIR = gauges
TIMERDIR = timers

# Core object files for partes-mpi (with MPI flag)
CORE_MPI_OBJS = partes-mpi-mpi.o parse_args-mpi.o get_tspec-mpi.o pterr-mpi.o stat-mpi.o detect_std_time-mpi.o

# Gauge object files for partes-mpi (with MPI flag)
GAUGE_MPI_OBJS = $(patsubst $(GAUGEDIR)/%.c,$(GAUGEDIR)/%-mpi.o,$(wildcard $(GAUGEDIR)/*.c))


# Kernel object files for partes-mpi (with MPI flag)
KERNEL_MPI_OBJS = $(KERNELDIR)/none-mpi.o $(KERNELDIR)/triad-mpi.o $(KERNELDIR)/scale-mpi.o \
                  $(KERNELDIR)/copy-mpi.o $(KERNELDIR)/add-mpi.o $(KERNELDIR)/pow-mpi.o \
                  $(KERNELDIR)/dgemm-mpi.o $(KERNELDIR)/mpi_bcast-mpi.o


# Timer object files for partes-mpi (with MPI flag)
TIMER_MPI_OBJS = $(patsubst $(TIMERDIR)/%.c,$(TIMERDIR)/%-mpi.o,$(wildcard $(TIMERDIR)/*.c))

# All object files for partes-mpi
PARTES_MPI_OBJS = $(CORE_MPI_OBJS) $(KERNEL_MPI_OBJS) $(TIMER_MPI_OBJS) $(GAUGE_MPI_OBJS)

# Object files for partes-fit (without MPI flag)
FIT_OBJS = partes-fit.o pterr.o stat.o detect_std_time.o get_tspec.o

# Targets
all: partes-mpi.x partes-fit.x

partes-mpi.x: $(PARTES_MPI_OBJS)
	$(CC) $(PARTES_MPI_OBJS) -o $@ $(LDFLAGS)

partes-fit.x: $(FIT_OBJS)
	$(CC) $(FIT_OBJS) $(TIMER_MPI_OBJS) -o $@ $(LDFLAGS)

tools:
	$(MAKE) -C tools

# Compilation rules for MPI versions (with PTOPT_USE_MPI flag)
%-mpi.o: %.c
	$(CC) $(CFLAGS) $(MPIFLAGS) -c $< -o $@

# Regular compilation rule (without MPI flag)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *-mpi.o $(KERNELDIR)/*.o $(TIMERDIR)/*.o $(GAUGEDIR)/*.o  *.x

cleanall: clean
	$(MAKE) -C tools clean
	rm *.csv
	rm utils/*.csv

.PHONY: all clean tools
