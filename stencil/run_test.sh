#!/bin/bash -x

narr=512
nt=100
tsc=2.494102
np=40
host=$(hostname)
kernel=jacobi2d5p

: "${PAPI_HOME:?PAPI_HOME is not set}"
: "${LIKWID_HOME:?LIKWID_HOME is not set}"
: "${OPENBLAS_HOME:?OPENBLAS_HOME is not set}"

LDFLAGS="-L${LIKWID_HOME}/lib/ -L${OPENBLAS_HOME}/lib/ -L${PAPI_HOME}/lib/"
CFLAGS="-I${LIKWID_HOME}/include/ -I${OPENBLAS_HOME}/include/ -I${PAPI_HOME}/include/"

#mpicc -O2 -Wall -o ${kernel}_tsc_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_TSC -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS}-lgsl -lopenblas
#mpicc -O2 -Wall -o ${kernel}_cgt_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_CGT -DNTEST=$nt -DNPASS=1  ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas
#mpicc -O2 -Wall -o ${kernel}_papi_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_PAPI -DNTEST=$nt -DNPASS=1  ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas -lpapi                               
#mpicc -O2 -Wall -o ${kernel}_papix6_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_PAPIX6 -DNTEST=$nt -DNPASS=1  ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas -lpapi                           
#mpicc -O2 -Wall -o ${kernel}_likwid_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_LIKWID -DLIKWID_PERFMON -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas -llikwid

mpicc -O2 -Wall -o ${kernel}_tsc.x ./${kernel}.c  -DTIMING -DUSE_TSC -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas
mpicc -O2 -Wall -o ${kernel}_cgt.x ./${kernel}.c  -DTIMING -DUSE_CGT -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas
mpicc -O2 -Wall -o ${kernel}_papi.x ./${kernel}.c  -DTIMING -DUSE_PAPI -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas -lpapi
mpicc -O2 -Wall -o ${kernel}_papix6.x ./${kernel}.c  -DTIMING -DUSE_PAPIX6 -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas -lpapi                        
mpicc -O2 -Wall -o ${kernel}_likwid.x ./${kernel}.c  -DTIMING -DUSE_LIKWID -DLIKWID_PERFMON -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas -llikwid  

for m in tsc cgt papi papix6
do
    mpirun --map-by core --bind-to core -np ${np} ./${kernel}_${m}.x $narr $tsc
    rm  -r ${kernel}${narr}n${nt}t_${m}_${host}
    mkdir ${kernel}${narr}n${nt}t_${m}_${host}
    mv ./*.csv ${kernel}${narr}n${nt}t_${m}_${host}
done

likwid-mpirun -mpi openmpi -np ${np} -g L3 -m ./${kernel}_likwid.x $narr $tsc
rm  -r ${kernel}${narr}n${nt}t_likwid_${host}
mkdir ${kernel}${narr}n${nt}t_likwid_${host}
mv ./*.csv ${kernel}${narr}n${nt}t_likwid_${host}

