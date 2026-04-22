#!/bin/bash -x

date +%Y-%m-%d_%H:%M:%S

narr=512
nt=100
# nspv=0.4166667
np=64
host=$(hostname)
kernel=jacobi2d5p

binw_min=10 # minimum width of a time bin in ns
p_low=0.01 # lowest threshold of probability of a data bin
nspv_ratio=0.8 #[Warning|#TODO]

PROJ_ROOT=$(realpath $(pwd)/..)
UTILS_ROOT=$(realpath "${PROJ_ROOT}/utils")
FILTER_ROOT=$(realpath ${PROJ_ROOT}/src/filter)

DO_SAMPLING=${DO_SAMPLING:-1}

echo "DO_SAMPLING: $DO_SAMPLING"

# TSC cycles/ns for RDTSC→ns (see print_tsc_freq.sh). Override method: TSC_METHOD=sysfs|calibrate|coarse
TSC_METHOD=${TSC_METHOD:-calibrate}
tsc=$("${UTILS_ROOT}/print_tsc_freq.sh" -m "${TSC_METHOD}" -q)
nspv=$(echo "scale=9; 1.0 / $tsc * $nspv_ratio" | bc)
echo "TSC FREQUENCY: $tsc"
echo "NSPV: $nspv"

:"${PAPI_HOME:?PAPI_HOME is not set}"
:"${LIKWID_HOME:?LIKWID_HOME is not set}"
:"${OPENBLAS_HOME:?OPENBLAS_HOME is not set}"

CFLAGS="-I${PAPI_HOME}/include/ -I${LIKWID_HOME}/include/ -I${OPENBLAS_HOME}.include"
LDFLAGS="-L${PAPI_HOME}/lib/ -L${LIKWID_HOME}/lib/ -L${OPENBLAS_HOME}/lib/"


mpicc -O2 -Wall -o ${kernel}_tsc_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_TSC -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas
mpicc -O2 -Wall -o ${kernel}_cgt_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_CGT -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas
mpicc -O2 -Wall -o ${kernel}_papi_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_PAPI -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas -lpapi                               
mpicc -O2 -Wall -o ${kernel}_papix6_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_PAPIX6 -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas -lpapi                           
mpicc -O2 -Wall -o ${kernel}_likwid_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_LIKWID -DLIKWID_PERFMON -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas -llikwid

mpicc -O2 -Wall -o ${kernel}_tsc.x ./${kernel}.c  -DTIMING -DUSE_TSC -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas
mpicc -O2 -Wall -o ${kernel}_cgt.x ./${kernel}.c  -DTIMING -DUSE_CGT -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas
mpicc -O2 -Wall -o ${kernel}_papi.x ./${kernel}.c  -DTIMING -DUSE_PAPI -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas -lpapi
mpicc -O2 -Wall -o ${kernel}_papix6.x ./${kernel}.c  -DTIMING -DUSE_PAPIX6 -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas -lpapi                        
mpicc -O2 -Wall -o ${kernel}_likwid.x ./${kernel}.c  -DTIMING -DUSE_LIKWID -DLIKWID_PERFMON -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lgsl -lopenblas -llikwid  

mpicc -O2 -Wall -o ${FILTER_ROOT}/filt.x ${FILTER_ROOT}/filt.c

for m in tsc cgt papi papix6 likwid
do
    for iarr in 64 128 256 512 1024
    do
        met_dir=${kernel}${iarr}n${nt}t_${m}_${host}
        tf_dir=${met_dir}_tf
        res_dir=${met_dir}_filt
        if [ "$DO_SAMPLING" -eq 1 ]; then
            rm -r $met_dir 2>/dev/null
            rm -r $tf_dir 2>/dev/null
            rm *.csv 2>/dev/null
            mkdir $met_dir
            mkdir $tf_dir
            mkdir $res_dir
        fi
        
        if [ "$DO_SAMPLING" -eq 1 ]; then
            mpirun --map-by core --bind-to core -np ${np} ./${kernel}_${m}.x $iarr $tsc
            mv ./*.csv $met_dir
        fi

        nsamp=`python3 ${FILTER_ROOT}/get_quantile.py ${met_dir} 1 0.5 ${nspv}`
        if [ "$DO_SAMPLING" -eq 1 ]; then
            mpirun --map-by core --bind-to core -np ${np} ./${kernel}_${m}_tf.x $iarr $nsamp $tsc
            mv ./*.csv $tf_dir
        fi

        python3 ${FILTER_ROOT}/get_met.py $met_dir 1    # get_met.py <met_dir> <data_col>
        python3 ${FILTER_ROOT}/get_tf.py $tf_dir 1 2 $nspv  # get_tf.py <tf_dir> <nsamp_col> <data_col> <nspv>
        binw=`python3 ${FILTER_ROOT}/get_binw.py ${met_dir} 1 $binw_min`    # get_binw.py <met_dir> <data_col> <binw_min>
        ${FILTER_ROOT}/filt.x -w $binw -n 100000 -l $p_low  # -w <binw> -n <nsamp> -l <p_low>
        mv met.csv tf.csv tr_hist.csv sim_cdf.csv $res_dir
    done
done

{ last -n 20; last | grep steill; }