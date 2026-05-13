#!/bin/bash -x

date +%Y-%m-%d_%H:%M:%S

narr=512
nt=100
# nspv=0.4166667
np=128
arch=$(uname -m)
host=${HOST_OVERRIDE:-$(hostname)}
# host=camd9554n2 # WIP.0506
# host=cgnr6760pn2 # WIP.0506

date_base=$(date +%Y%m%d)
DATA_ROOT=~/code/data/${date_base}/${host}/output_sampOpt_filtOpt/

if [ ! -d $DATA_ROOT ]; then
    mkdir -p $DATA_ROOT
fi
DATA_ROOT=$(realpath $DATA_ROOT)

# kernel=tl_f90_cg_calc_w
kernel=jacobi2d5p

binw_min=${BINW_MIN:-10} # minimum width of a time bin in ns
# binw_override=${BINW_OVERRIDE:-10}
p_low=${P_LOW:-0.005} # lowest threshold of probability of a data bin
nspv_ratio=1.0 #[Warning|#TODO]
filt_nsamp=${FILT_NSAMP:-200000}
IARR_LIST=${IARR_LIST:-1024}

PROJ_ROOT=$(realpath $(pwd)/..)
UTILS_ROOT=$(realpath "${PROJ_ROOT}/utils")
FILTER_ROOT=$(realpath ${PROJ_ROOT}/src/filter)

DO_SAMPLING=${DO_SAMPLING:-1}

echo "DO_SAMPLING: $DO_SAMPLING"
echo "HOST: $host"
echo "IARR_LIST: $IARR_LIST"
echo "BINW_MIN: $binw_min"
echo "BINW_OVERRIDE: $binw_override"
echo "P_LOW: $p_low"
echo "FILT_NSAMP: $filt_nsamp"

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

rm -rf *.x
if [ ${host} == "x86_64" ]; then
    LDFLAGS="${LDFLAGS} -lgsl -lopenblas"
    mpicc -O2 -Wall -o ${kernel}_tsc_tf.x ./${kernel}.c  -DSTAGE_TF -DTIMING -DUSE_TSC -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} 
    mpicc -O2 -Wall -o ${kernel}_likwid_tf.x ./${kernel}.c  -DSTAGE_TF -DTIMING -DUSE_LIKWID -DLIKWID_PERFMON -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -llikwid

    mpicc -O2 -Wall -o ${kernel}_tsc.x ./${kernel}.c  -DTIMING -DUSE_TSC -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS}
    mpicc -O2 -Wall -o ${kernel}_likwid.x ./${kernel}.c  -DTIMING -DUSE_LIKWID -DLIKWID_PERFMON -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -llikwid  
fi

if [ ${arch} == "aarch64" ]; then
    mpicc -O2 -Wall -o ${kernel}_cntvct_tf.x ./${kernel}.c  -DSTAGE_TF -DTIMING -DUSE_CNTVCT -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS}
    mpicc -O2 -Wall -o ${kernel}_cntvct_fence_tf.x ./${kernel}.c  -DSTAGE_TF -DTIMING -DUSE_CNTVCT_FENCE -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS}
    mpicc -O2 -Wall -o ${kernel}_cntvcto_tf.x ./${kernel}.c  -DSTAGE_TF -DTIMING -DUSE_CNTVCTO -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS}

    mpicc -O2 -Wall -o ${kernel}_cntvct.x ./${kernel}.c  -DTIMING -DUSE_CNTVCT -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS}
    mpicc -O2 -Wall -o ${kernel}_cntvct_fence.x ./${kernel}.c  -DTIMING -DUSE_CNTVCT_FENCE -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS}
    mpicc -O2 -Wall -o ${kernel}_cntvcto.x ./${kernel}.c  -DTIMING -DUSE_CNTVCTO -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS}
fi

mpicc -O2 -Wall -o ${kernel}_cgt_tf.x ./${kernel}.c  -DSTAGE_TF -DTIMING -DUSE_CGT -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS}
mpicc -O2 -Wall -o ${kernel}_papi_tf.x ./${kernel}.c  -DSTAGE_TF -DTIMING -DUSE_PAPI -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lpapi                               
mpicc -O2 -Wall -o ${kernel}_papix6_tf.x ./${kernel}.c  -DSTAGE_TF -DTIMING -DUSE_PAPIX6 -DNTEST=$nt -DNPASS=1 ${CFLAGS} ${LDFLAGS} -lpapi                           


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

if [ -z "${TIMER_LIST:-}" ]; then
    TIMER_LIST="cgt papi papix6"
    if [ ${arch} == "x86_64" ]; then
        TIMER_LIST="${TIMER_LIST} tsc likwid"
    fi

    if [ ${arch} == "aarch64" ]; then
        TIMER_LIST="${TIMER_LIST} cntvct cntvct_fence cntvcto"
    fi
fi
echo "TIMER_LIST: $TIMER_LIST"

# TIMER_LIST="cntvcto"
for m in ${TIMER_LIST}
do
    for iarr in ${IARR_LIST}
    do
        met_dir="${DATA_ROOT}/${kernel}${iarr}n${nt}t_${m}_${host}"
        tf_dir="${met_dir}_tf"
        res_dir="${met_dir}_filt"
        
        if [ "$DO_SAMPLING" -eq 1 ]; then
            rm -r $met_dir 2>/dev/null
            rm -r $tf_dir 2>/dev/null
            rm *.csv 2>/dev/null
        fi
        mkdir -p $met_dir
        mkdir -p $tf_dir
        rm -rf $res_dir
        mkdir -p $res_dir
        
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
        mv met.csv tf.csv
        head -n 10 tf.csv
        
        python3 ${FILTER_ROOT}/get_met.py $tf_dir 1 # get_tf.py <tf_dir> <nsamp_col> <data_col> <nspv>
        head -n 10 met.csv

        if [ -n "$binw_override" ]; then
            binw=$binw_override
        else
            binw=`python3 ${FILTER_ROOT}/get_binw.py ${met_dir} 1 $binw_min`    # get_binw.py <met_dir> <data_col> <binw_min>
        fi
        echo "BINW: $binw"
        ${FILTER_ROOT}/filt.x -w $binw -n $filt_nsamp -l $p_low -x 0.005 -y 0.005 -z 0.005 # -w <binw> -n <nsamp> -l <p_low>
        mv met.csv tf.csv tr_hist.csv tm_hist.csv sim_cdf.csv er.out ep.out wd.out $res_dir
    done
done

date +%Y-%m-%d_%H:%M:%S

{ last -n 20; last | grep steill; }
