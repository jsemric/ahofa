#!/bin/bash

for i in $@;do
    states="`cat $i| awk '{print $1"\n"$2}' | grep -ox "[0-9]\+" | sort -u | wc -l`"
    transitions="`cat $i | grep "0x" | wc -l`"
    echo "$states $transitions $i"
done
