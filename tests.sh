#!/bin/bash

count=0
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

for filename in $(ls CompArchHw1Tests/test*.trc | sort -V); do
    if [ $count -ge 200 ]; then
        break
    fi
    test_num=$(echo "$filename" | cut -d'.' -f1)
    dos2unix "$filename" > /dev/null 2>&1
    ./bp_main "$filename" > "${test_num}Yours.out"
    diff_output=$(diff -u "${test_num}.out" "${test_num}Yours.out")
    if [ $? -ne 0 ]; then
        echo -e "${RED}Test ${test_num} failed:${NC}"
        echo "$diff_output" | colordiff
        exit 1
    fi
        echo "Running test ${test_num}"
    count=$((count + 1))
done


for filename in $(ls tests_itai_idan_CA/example*.trc | sort -V); do
    if [ $count -ge 200 ]; then
        break
    fi
    test_num=$(echo "$filename" | cut -d'.' -f1)
    dos2unix "$filename" > /dev/null 2>&1
    ./bp_main "$filename" > "${test_num}Yours.out"
    diff_output=$(diff -u "${test_num}.out" "${test_num}Yours.out")
    if [ $? -ne 0 ]; then
        echo -e "${RED}Test ${test_num} failed:${NC}"
        echo "$diff_output" | colordiff
        exit 1
    fi
        echo "Running test ${test_num}"
    count=$((count + 1))
done


let i=4; while((i<150)); do ./bp_main tests/example${i}.trc > tests/your_output_for_example${i}.out; let i++; done

let i=4; while((i<150)); do diff -s -q tests/example${i}.out tests/your_output_for_example${i}.out; let i++; done

let i=4; while((i<150)); do ./bp_main tests/n_example${i}.trc > tests/your_output_for_n_example${i}.out; let i++; done

let i=4; while((i<150)); do diff -s -q tests/n_example${i}.out tests/your_output_for_n_example${i}.out; let i++; done

let i=4; while((i<150)); do ./bp_main tests/small_btb_example${i}.trc > tests/your_output_for_small_btb_example${i}.out; let i++; done

let i=4; while((i<150)); do diff -s -q tests/small_btb_example${i}.out tests/your_output_for_small_btb_example${i}.out; let i++; done

let i=1; while((i<21)); do ./bp_main tests/segel/example${i}.trc > tests/segel/your_segel_example${i}.out; let i++; done

let i=1; while((i<21)); do diff -s -q tests/segel/example${i}.out tests/segel/your_segel_example${i}.out; let i++; done

let i=1; while((i<4)); do ./bp_main input_examples/example${i}.trc > input_examples/your_output_example${i}.out; let i++; done

let i=1; while((i<4)); do diff -s -q input_examples/your_output_example${i}.out input_examples/example${i}.out; let i++; done