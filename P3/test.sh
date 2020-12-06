make
for alg in 1 2
do
  for test in 1 2 3 4 5 6 7
  do
    echo "Page Replacement Algorithm:" $alg " Test: " $test
    diff ./TestOutputs/$alg/input$test.out  <(./out $alg ./TestInputs/input$test)
  done
done
