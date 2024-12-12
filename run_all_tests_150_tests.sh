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