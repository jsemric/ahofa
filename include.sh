#!/bin/bash

if [ "$#" -ne 2 ];then
    echo "Error: two arguments required" >&2
    exit 1
fi

TMP1="tmptmptmp1"
TMP2="tmptmptmp2"
./minimize_nfa.py -i $1 -o $TMP1 -b
./minimize_nfa.py -i $2 -o $TMP2 -b
java -jar RABIT.jar $TMP1 $TMP2 -fast -finite
rm -f $TMP1 $TMP2
