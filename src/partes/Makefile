CC = mpicc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = -lm

# Source directories
KERNELDIR = kernels
TIMERDIR = timers

# Core object files for partes
CORE_OBJS = partes.o parse_args.o stress_timer.o pterr.o stat.o detect_std_time.o

# Kernel object files
KERNEL_OBJS = $(KERNELDIR)/none.o $(KERNELDIR)/triad.o $(KERNELDIR)/scale.o \
              $(KERNELDIR)/copy.o $(KERNELDIR)/add.o $(KERNELDIR)/pow.o \
              $(KERNELDIR)/dgemm.o $(KERNELDIR)/bcast.o

# Timer object files
TIMER_OBJS = $(wildcard $(TIMERDIR)/*.o)

# All object files for partes
PARTES_OBJS = $(CORE_OBJS) $(KERNEL_OBJS) $(TIMER_OBJS)

# Object files for partes-fit
FIT_OBJS = partes-fit.o pterr.o stat.o detect_std_time.o stress_timer.o

# Targets
all: partes.x partes-fit.x

partes.x: $(PARTES_OBJS)
	$(CC) $(PARTES_OBJS) -o $@ $(LDFLAGS)

partes-fit.x: $(FIT_OBJS)
	$(CC) $(FIT_OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(KERNELDIR)/*.o $(TIMERDIR)/*.o partes.x partes-fit.x

.PHONY: all clean