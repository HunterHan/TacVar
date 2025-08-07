CC = mpicc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = -lm

# Source files
SRCDIR = .
KERNELDIR = kernels
TIMERDIR = timers

# Main source files
MAIN_SRC = $(SRCDIR)/partes.c
PARSE_SRC = $(SRCDIR)/parse_args.c
STRESS_SRC = $(SRCDIR)/stress_timer.c
PTERR_SRC = $(SRCDIR)/pterr.c

# Kernel source files
KERNEL_SRCS = $(KERNELDIR)/none.c \
               $(KERNELDIR)/triad.c \
               $(KERNELDIR)/scale.c \
               $(KERNELDIR)/copy.c \
               $(KERNELDIR)/add.c \
               $(KERNELDIR)/pow.c \
               $(KERNELDIR)/dgemm.c \
               $(KERNELDIR)/bcast.c

# Timer source files (if any)
TIMER_SRCS = $(wildcard $(TIMERDIR)/*.c)

# All source files
ALL_SRCS = $(MAIN_SRC) $(PARSE_SRC) $(STRESS_SRC) $(PTERR_SRC) $(KERNEL_SRCS) $(TIMER_SRCS)

# Object files
OBJS = $(ALL_SRCS:.c=.o)

# Target executable
TARGET = partes

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)

# Install (optional)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# Uninstall (optional)
uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Dependencies
$(SRCDIR)/partes.o: $(SRCDIR)/partes.c $(SRCDIR)/partes_types.h $(SRCDIR)/pterr.h $(KERNELDIR)/kernels.h
$(SRCDIR)/parse_args.o: $(SRCDIR)/parse_args.c $(SRCDIR)/partes_types.h $(KERNELDIR)/kernels.h
$(SRCDIR)/stress_timer.o: $(SRCDIR)/stress_timer.c
$(SRCDIR)/pterr.o: $(SRCDIR)/pterr.c $(SRCDIR)/pterr.h

# Kernel dependencies
$(KERNELDIR)/none.o: $(KERNELDIR)/none.c
$(KERNELDIR)/triad.o: $(KERNELDIR)/triad.c
$(KERNELDIR)/scale.o: $(KERNELDIR)/scale.c
$(KERNELDIR)/copy.o: $(KERNELDIR)/copy.c
$(KERNELDIR)/add.o: $(KERNELDIR)/add.c
$(KERNELDIR)/pow.o: $(KERNELDIR)/pow.c
$(KERNELDIR)/dgemm.o: $(KERNELDIR)/dgemm.c
$(KERNELDIR)/bcast.o: $(KERNELDIR)/bcast.c

.PHONY: all clean install uninstall
