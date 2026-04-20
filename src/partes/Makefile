CC ?= mpicc

CFLAGS = -Wall -Wextra -std=c99 -O2 -save-temps=obj

# --- Dependencies ---
# Prefer env.bash to export PAPI_HOME/LIKWID_HOME/USE_*.
PAPI_PREFIX ?= $(PAPI_HOME)
LIKWID_PREFIX ?= $(LIKWID_HOME)

# Optional local prefix (disabled by default). If enabled, it is appended.
USE_LOCAL ?= 0
LOCAL_PREFIX ?= $(HOME)/local

INCLUDES =
LIBDIRS  =
LIBS     = -lm

ifeq ($(USE_LOCAL),1)
  INCLUDES += -I$(LOCAL_PREFIX)/include
  LIBDIRS  += -L$(LOCAL_PREFIX)/lib -L$(LOCAL_PREFIX)/lib64
endif

TSC_CHECK := $(shell lscpu |grep -i tsc 2>/dev/null)
ifneq ($(strip $(TSC_CHECK)),)
  USE_TSC := 1
else
  USE_TSC := 0
endif

# Enable PAPI by setting USE_PAPI=1 (and ensure PAPI_PREFIX is correct).
# If headers/libs are missing, automatically force USE_PAPI=0.
USE_PAPI ?= 0
ifneq ($(USE_PAPI),0)
  ifeq ($(wildcard $(PAPI_PREFIX)/include/papi.h),)
    USE_PAPI := 0
  endif
endif
ifeq ($(USE_PAPI),1)
  MPIFLAGS += -DUSE_PAPI
  INCLUDES += -I$(PAPI_PREFIX)/include
  LIBDIRS  += -L$(PAPI_PREFIX)/lib -L$(PAPI_PREFIX)/lib64
  LIBS     += -lpapi
endif

# Enable LIKWID by setting USE_LIKWID=1 (and ensure LIKWID_PREFIX is correct).
# If headers/libs are missing, automatically force USE_LIKWID=0.
USE_LIKWID ?= 0
ifneq ($(USE_LIKWID),0)
  ifeq ($(wildcard $(LIKWID_PREFIX)/include/likwid.h),)
    USE_LIKWID := 0
  endif
endif
# Enable LIKWID by setting USE_LIKWID=1 (and ensure LIKWID_PREFIX is correct).
ifeq ($(USE_LIKWID),1)
  MPIFLAGS += -DUSE_LIKWID
  INCLUDES += -I$(LIKWID_PREFIX)/include
  LIBDIRS  += -L$(LIKWID_PREFIX)/lib -L$(LIKWID_PREFIX)/lib64
  LIBS     += -llikwid
endif

LDFLAGS = $(INCLUDES) $(LIBDIRS) $(LIBS)
MPIFLAGS += -DPTOPT_USE_MPI

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
TIMER_SRCS = $(TIMERDIR)/clock_gettime.c $(TIMERDIR)/mpi_wtime.c
ifeq ($(USE_PAPI),1)
  TIMER_SRCS += $(TIMERDIR)/papi_timer.c $(TIMERDIR)/papix6_timer.c
endif
ifeq ($(USE_LIKWID),1)
  TIMER_SRCS += $(TIMERDIR)/likwid_timer.c
endif
ifeq ($(USE_TSC),1)
  TIMER_SRCS += $(TIMERDIR)/tsc_asym.c
endif

TIMER_SRCS += $(TIMERDIR)/tsc_asym.c
TIMER_MPI_OBJS = $(patsubst $(TIMERDIR)/%.c,$(TIMERDIR)/%-mpi.o,$(TIMER_SRCS))

# All object files for partes-mpi
PARTES_MPI_OBJS = $(CORE_MPI_OBJS) $(KERNEL_MPI_OBJS) $(TIMER_MPI_OBJS) $(GAUGE_MPI_OBJS)

# Object files for partes-fit (without MPI flag)
FIT_OBJS = partes-fit.o pterr.o stat.o detect_std_time.o get_tspec.o

# Targets
all: partes-mpi.x partes-fit.x

partes-mpi.x: $(PARTES_MPI_OBJS)
	$(CC) -g $(PARTES_MPI_OBJS) -o $@ $(LDFLAGS)

partes-fit.x: $(FIT_OBJS)
	$(CC) -g $(FIT_OBJS) $(TIMER_MPI_OBJS) -o $@ $(LDFLAGS)

tools:
	$(MAKE) -C tools

# Compilation rules for MPI versions (with PTOPT_USE_MPI flag)
%-mpi.o: %.c
	$(CC) -g $(CFLAGS) $(MPIFLAGS) $(LDFLAGS) -c $< -o $@

# Regular compilation rule (without MPI flag)
%.o: %.c
	$(CC) -g $(CFLAGS) $(LDFLAGS) -c $< -o $@

clean:
	rm -f *.o *-mpi.o $(KERNELDIR)/*.o $(TIMERDIR)/*.o $(GAUGEDIR)/*.o  *.x

cleanall: clean
	$(MAKE) -C tools clean
	rm *.csv
	rm utils/*.csv

.PHONY: all clean tools
