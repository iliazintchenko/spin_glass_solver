#!/bin/bash

#
# usage
#
echo "Usage : %1:Session name ($1)"
echo "        %2:Hours needed ($2)"
echo "        %3:Minutes needed ($3)"
echo "        %4:server-num-nodes ($4)"
echo "        %5:server-ip:port ($5)"
echo "        %6:partition ($6)"
echo "        %7:account ($7)"
echo "        %8:reservation (${8})"
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
ACCOUNT=""
RESERVATION=""
# create strings for partition and reservation
if [ "${6}" != "" ]
then
 PARTITION='#SBATCH --partition='${6}
fi
if [ "${7}" != "" ]
then
ACCOUNT='#SBATCH --account='${7}
fi
if [ "${8}" != "" ]
then
 RESERVATION='#SBATCH --reservation='${8}
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
$ACCOUNT
$RESERVATION

module load gcc/4.8.2
module load mpc/1.0.2
module load mpfr/3.1.2
module load gmp/5.1.3


cd /gpfs/bbp.cscs.ch/home/biddisco/gcc/bgas/build/spinmaster

CMD="srun --ntasks=$NSERVERS --ntasks-per-node=1 bin/spinsolve --hpx:ignore-batch-env --hpx:agas=$5 --hpx:connect --hpx:threads=16"
echo "command to run is "\$CMD

eval "\$CMD"

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

