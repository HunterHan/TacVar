CC = mpicc
CFLAGS = -Wall -Wextra -std=c99 -O3
LDFLAGS = -lm
TIMERDIR = ../timers

TIMER_OBJS = $(patsubst $(TIMERDIR)/%.c,$(TIMERDIR)/%.o,$(wildcard $(TIMERDIR)/*.c))

# All tool targets
all: meas_single.x meas_series_wd.x meas_pair.x timer_model_fit.x

meas_single.x: meas_single.o
	$(CC) meas_single.o -o $@ $(LDFLAGS)

meas_series_wd.x: meas_series_wd.o
	$(CC) meas_series_wd.o -o $@ $(LDFLAGS)

meas_pair.x: meas_pair.o
	$(CC) meas_pair.o -o $@ $(LDFLAGS)

timer_model_fit.x: timer_model_fit.o $(TIMER_OBJS)
	$(CC) timer_model_fit.o $(TIMER_OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -I.. -c $< -o $@

clean:
	rm -f *.o *.x

.PHONY: all clean
