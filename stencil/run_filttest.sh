#!/bin/bash -x
pkill -u $(whoami) -9 "mpirun" 2>/dev/null

narr=512
nt=100
tsc=2.494102
nspv=0.4166667
np=40
host=cas114
kernel=jacobi2d5p

rm *\.x

mpicc -O2 -Wall -o ${kernel}_tsc_tf.x ./${kernel}_merged.c -DUSE_TF -DTIMING -DUSE_TSC -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas
mpicc -O2 -Wall -o ${kernel}_cgt_tf.x ./${kernel}_merged.c -DUSE_TF -DTIMING -DUSE_CGT -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas
mpicc -O2 -Wall -o ${kernel}_papi_tf.x ./${kernel}_merged.c -DUSE_TF -DTIMING -DUSE_PAPI -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas -lpapi                               
mpicc -O2 -Wall -o ${kernel}_papix6_tf.x ./${kernel}_merged.c -DUSE_TF -DTIMING -DUSE_PAPIX6 -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas -lpapi                           
#mpicc -O2 -Wall -o ${kernel}_likwid_tf.x ./${kernel}_merged.c -DUSE_TF -DTIMING -DUSE_LIKWID -DLIKWID_PERFMON -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas -llikwid

mpicc -O2 -Wall -o ${kernel}_tsc.x ./${kernel}_merged.c  -DTIMING -DUSE_TSC -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas
mpicc -O2 -Wall -o ${kernel}_cgt.x ./${kernel}_merged.c  -DTIMING -DUSE_CGT -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas
mpicc -O2 -Wall -o ${kernel}_papi.x ./${kernel}_merged.c  -DTIMING -DUSE_PAPI -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas -lpapi
mpicc -O2 -Wall -o ${kernel}_papix6.x ./${kernel}_merged.c  -DTIMING -DUSE_PAPIX6 -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas -lpapi                        
#mpicc -O2 -Wall -o ${kernel}_likwid.x ./${kernel}_merged.c  -DTIMING -DUSE_LIKWID -DLIKWID_PERFMON -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas -llikwid  

output_dir=$(date +"%Y%m%d_%H%M%S")
mkdir $output_dir

for m in tsc
do
    for iarr in 64
    do
        met_dir=$output_dir/${kernel}${iarr}n${nt}t_${m}_${host}
        tf_dir=${met_dir}_tf
        res_dir=${met_dir}_filt
        rm  -r $met_dir
        rm  -r $tf_dir
        rm *.csv
        mkdir $met_dir
        mkdir $tf_dir
        mkdir $res_dir
        mpirun --map-by core --bind-to core -np ${np} ./${kernel}_${m}.x $iarr $tsc
        mv ./*.csv $met_dir
        nsamp=`python3 ../src/filter/get_quantile.py ${met_dir} 1 0.5 ${nspv}`
        mpirun --map-by core --bind-to core -np ${np} ./${kernel}_${m}_tf.x $iarr $nsamp $tsc
        mv ./*.csv $tf_dir
        python3 ../src/filter/get_met.py $met_dir 1
        python3 ../src/filter/get_tf.py $tf_dir 1 2 $nspv
        binw=`python3 ../src/filter/get_binw.py ${met_dir} 1`
        pushd ../src/filter
        rm -rf filt.x
        gcc -o filt.x filt.o -lm
        popd
        cp -r ../src/filter/filt.x ./
        ./filt.x -w $binw -n 100000 -l 0.01
        mv met.csv tf.csv tr_hist.csv sim_cdf.csv $res_dir
    done
done


