make
for cpu in 0 1 2 3
do
  for test in 0 1 2 3
  do
    echo "CPU Type:" $cpu " Test: " $test
    diff ./TestOutputs/$cpu/input_$test.out  <(./out $cpu ./TestInputs/input_$test)
  done
done
