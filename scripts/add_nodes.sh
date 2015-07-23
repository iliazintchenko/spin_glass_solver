#!/bin/bash

#
# usage
#
echo "Usage : %1:Session name ($1)"
echo "        %2:Hours needed ($2)"
echo "        %3:Minutes needed ($3)"
echo "        %4:server-num-nodes ($4)"
echo "        %5:server-ip:port ($5)"
echo "        %6:Partition ($6)"
echo "        %7:reservation (${7})"
echo ""

# -------------------------------------------------------------------
# Function to create job script from params
# -------------------------------------------------------------------

function write_script
{
# setup vars we will use
JOB_NAME=$1
NSERVERS=$[ $4 ]

echo "Nservers is "$NSERVERS
PARTITION=""
RESERVATION=""
# create strings for partition and reservation
if [ "${6}" != "" ]
then
 PARTITION='#SBATCH --partition='${6}
fi
if [ "${7}" != "" ]
then
 RESERVATION='#SBATCH --reservation='${7}
fi

# create the job script
echo "Creating job $JOB_NAME"

#====== START of JOB SCRIPT =====
cat << _EOF_ > ./submit-job.bash
#!/bin/bash

#SBATCH --job-name=$JOB_NAME
#SBATCH --time=$2:$3:00
#SBATCH --nodes=$4
$PARTITION
$RESERVATION

module load gcc/4.9.0

export LD_LIBRARY_PATH=/apps/monch/mpc/1.0.1/lib:/apps/monch/mpfr/3.1.2/lib:/apps/monch/gmp/5.1.2/lib:/apps/monch/mpc/1.0.1/lib:/apps/monch/mpfr/3.1.2/lib:/apps/monch/gmp/5.1.2/lib:/apps/monch/git/1.8.4.1/lib:/apps/monch/cmake/2.8.11.2/lib:/apps/monch/gcc/4.9.0/lib64:/apps/monch/gmp/5.1.2/lib:/apps/monch/mpfr/3.1.2/lib:/apps/monch/mpc/1.0.1/lib

cd /mnt/lnec/biddisco/build/spinmaster

CMD="srun --ntasks=$NSERVERS --ntasks-per-node=1 bin/spinsolve  --hpx:ignore-batch-env --hpx:agas=$5 --hpx:connect"
echo "command to run is "\$CMD

eval "\$CMD"
#srun -n $NSERVERS -N 1 bin/spinsolve --hpx:agas=$5 --hpx:connect

_EOF_

#====== END of JOB SCRIPT =====

chmod 775 ./submit-job.bash

}

# -------------------------------------------------------------------
# End function
# -------------------------------------------------------------------

write_script $1 $2 $3 $4 $5 $6 $7 $8 $9 ${10}

echo "Job script written"
cat ./submit-job.bash


#
# submit the job
#
sbatch ./submit-job.bash

#
# wipe the temp file
#
#rm ./submit-job.bash

