#!/bin/bash -l

#$ -N zkfhe_v3
#$ -P he
#$ -pe omp 28
#$ -l h_rt=04:00:00
#$ -m beas
#$ -j y
#$ -o zkfhe_v3_$JOB_ID.log

echo "===== Job Info ====="
echo "Job ID:    $JOB_ID"
echo "Host:      $(hostname)"
echo "Cores:     $NSLOTS"
echo "Start:     $(date)"
echo "===================="

REPO=/projectnb/he/Lohit/crypto/ZKP/zkOpenFHE
BUILD=$REPO/build

# ---- Build on the compute node to match its architecture ----
echo ""
echo "===== Building on compute node ====="
cd $BUILD
cmake .. 2>&1 | tail -5
cmake --build . --target verifiable-simple-integers-bgvrns-v3 -j$NSLOTS 2>&1 | tail -10
BUILD_RC=$?

if [ $BUILD_RC -ne 0 ]; then
    echo "BUILD FAILED (exit code $BUILD_RC), aborting."
    exit $BUILD_RC
fi
echo "Build: done"

# ---- Run ----
echo ""
echo "===== Running verifiable-simple-integers-bgvrns-v3 ====="
export OMP_NUM_THREADS=$NSLOTS

./bin/examples/pke/verifiable-simple-integers-bgvrns-v3
RUN_RC=$?

echo ""
echo "Exit code: $RUN_RC"
echo "End:       $(date)"
exit $RUN_RC
