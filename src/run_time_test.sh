#! /bin/sh

make time_test

echo
echo "Running time test for sequential"
./time_test --grid_size $1 --impl seq --num_threads $2

echo
echo "Running time test for omp"
./time_test --grid_size $1 --impl omp --num_threads $2

echo
echo "Running time test for PThreads"
./time_test --grid_size $1 --impl pth --num_threads $2