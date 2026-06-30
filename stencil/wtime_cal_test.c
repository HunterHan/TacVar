#include <mpi.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
static uint64_t cgt_ns(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000000ULL+t.tv_nsec; }
int main(int argc,char**argv){
  MPI_Init(&argc,&argv);
  printf("MPI_Wtick = %g s\n", MPI_Wtick());
  for(int trial=0;trial<4;trial++){
    /* busy interval (mirrors a compute kernel) */
    uint64_t c0=cgt_ns(); double w0=MPI_Wtime();
    volatile double s=0; for(long i=0;i<300000000L;i++) s+=i*0.5;
    uint64_t c1=cgt_ns(); double w1=MPI_Wtime();
    double cgt_d=(double)(c1-c0)/1e9, wt_d=(w1-w0);
    /* nanosleep interval (pure wall clock) */
    struct timespec req={0,500000000L};
    uint64_t cs0=cgt_ns(); double ws0=MPI_Wtime();
    nanosleep(&req,NULL);
    uint64_t cs1=cgt_ns(); double ws1=MPI_Wtime();
    double cgt_sl=(double)(cs1-cs0)/1e9, wt_sl=(ws1-ws0);
    printf("[busy ] cgt=%.6f  wtime=%.6f  cgt/wt=%.4f   [sleep0.5] cgt=%.6f wtime=%.6f cgt/wt=%.4f\n",
           cgt_d, wt_d, cgt_d/wt_d, cgt_sl, wt_sl, cgt_sl/wt_sl);
  }
  MPI_Finalize(); return 0;
}
