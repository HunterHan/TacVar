#!/bin/bash -x

date +%Y-%m-%d_%H:%M:%S

narr=512
nt=100
# nspv=0.4166667
np=64
host=$(hostname)
# host=camd9554n2 # WIP.0506
# host=cgnr6760pn2 # WIP.0506

DATA_ROOT=$(realpath ~/code/data/20260423/${host}/output)

# kernel=tl_f90_cg_calc_w
kernel=jacobi2d5p

binw_min=10 # minimum width of a time bin in ns
p_low=0.01 # lowest threshold of probability of a data bin
nspv_ratio=1.0 #[Warning|#TODO]

PROJ_ROOT=$(realpath $(pwd)/..)
UTILS_ROOT=$(realpath "${PROJ_ROOT}/utils")
FILTER_ROOT=$(realpath ${PROJ_ROOT}/src/filter)

DO_SAMPLING=${DO_SAMPLING:-1}

echo "DO_SAMPLING: $DO_SAMPLING"

if [ ${host} == "c920bn3" ]; then
    tsc=2.900000
else
    # TSC cycles/ns for RDTSC→ns (see print_tsc_freq.sh). Override method: TSC_METHOD=sysfs|calibrate|coarse
    TSC_METHOD=${TSC_METHOD:-calibrate}
    tsc=$("${UTILS_ROOT}/print_tsc_freq.sh" -m "${TSC_METHOD}" -q)
fi
# # 
# tsc=2.2 # WIP.0506

nspv=$(echo "scale=9; 1.0 / $tsc * $nspv_ratio" | bc)
echo "TSC FREQUENCY: $tsc"
echo "NSPV: $nspv"

if [ "$DO_SAMPLING" -eq 1 ]; then
:"${PAPI_HOME:?PAPI_HOME is not set}"
:"${LIKWID_HOME:?LIKWID_HOME is not set}"
:"${OPENBLAS_HOME:?OPENBLAS_HOME is not set}"

CFLAGS="-I${PAPI_HOME}/include/ -I${LIKWID_HOME}/include/ -I${OPENBLAS_HOME}.include"
LDFLAGS="-L${PAPI_HOME}/lib/ -L${LIKWID_HOME}/lib/ -L${OPENBLAS_HOME}/lib/"
if [ ${host} != "c920bn3" ]; then
    LDFLAGS="${LDFLAGS} -lgsl -lopenblas"
    mpicc -O2 -Wall -o ${kernel}_tsc_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_TSC -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} 
    mpicc -O2 -Wall -o ${kernel}_likwid_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_LIKWID -DLIKWID_PERFMON -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -llikwid

    mpicc -O2 -Wall -o ${kernel}_tsc.x ./${kernel}.c  -DTIMING -DUSE_TSC -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS}
    mpicc -O2 -Wall -o ${kernel}_likwid.x ./${kernel}.c  -DTIMING -DUSE_LIKWID -DLIKWID_PERFMON -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -llikwid  
fi



mpicc -O2 -Wall -o ${kernel}_cgt_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_CGT -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS}
mpicc -O2 -Wall -o ${kernel}_papi_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_PAPI -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lpapi                               
mpicc -O2 -Wall -o ${kernel}_papix6_tf.x ./${kernel}_tf.c  -DTIMING -DUSE_PAPIX6 -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lpapi                           


mpicc -O2 -Wall -o ${kernel}_cgt.x ./${kernel}.c  -DTIMING -DUSE_CGT -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS}
mpicc -O2 -Wall -o ${kernel}_papi.x ./${kernel}.c  -DTIMING -DUSE_PAPI -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lpapi
mpicc -O2 -Wall -o ${kernel}_papix6.x ./${kernel}.c  -DTIMING -DUSE_PAPIX6 -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lpapi                        

if [ ${host} == "camd9554n2" ]; then
    sudo cpupower frequency-set -u 3.1GHz -d 3.1GHz
elif [ ${host} == "cgnr6760pn2" ]; then
    sudo cpupower frequency-set -u 2.2GHz -d 2.2GHz
elif [ ${host} == "c920bn3" ]; then
    sudo cpupower frequency-set -u 2.9GHz -d 2.9GHz
fi

sudo cpupower frequency-info
fi

gcc -O2 -Wall -o ${FILTER_ROOT}/filt.x ${FILTER_ROOT}/filt.c

TIMER_LIST="cgt papi papix6"
if [ ${host} != "c920bn3" ]; then
    TIMER_LIST="${TIMER_LIST} tsc likwid"
fi

for m in ${TIMER_LIST}
do
    for iarr in 64 128 256 512 1024
    do
        met_dir=$DATA_ROOT/${kernel}${iarr}n${nt}t_${m}_${host}
        tf_dir=${met_dir}_tf
        res_dir=${met_dir}_filt
        if [ "$DO_SAMPLING" -eq 1 ]; then
            rm -r $met_dir 2>/dev/null
            rm -r $tf_dir 2>/dev/null
            rm *.csv 2>/dev/null
        fi
        mkdir $met_dir
        mkdir $tf_dir
        mkdir $res_dir
        
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
        ${FILTER_ROOT}/filt.x -w $binw -n 100000 -l $p_low -x 0.005 -y 0.005 -z 0.005 # -w <binw> -n <nsamp> -l <p_low>
        mv met.csv tf.csv tr_hist.csv tm_hist.csv sim_cdf.csv er.out ep.out wd.out $res_dir
    done
done

date +%Y-%m-%d_%H:%M:%S

{ last -n 20; last | grep steill; }