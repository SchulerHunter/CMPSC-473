make
for mem in 0 1
do
  for test in 1 2 3 4 5 6
  do
    echo "Allocator Scheme:" $mem " Test: " $test
    diff ./TestOutputs/$mem/output_$test  <(./out $mem ./TestInputs/input_$test)
  done
done
