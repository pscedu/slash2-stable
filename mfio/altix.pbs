#!/bin/bash
#PBS -j oe -l ncpus=16,walltime=50:00

/bin/date +"START TIME: %m/%d/%Y %H:%M:%S"
ja
set -x

cd $PBS_O_WORKDIR

mpirun ./fio.zmpi -i ./example.in -o ./zmpi.out

set +x
/bin/date +"END TIME: %m/%d/%Y %H:%M:%S"
ja -chlst
