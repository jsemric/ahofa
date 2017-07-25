#!/bin/bash

for i in $@;do
    echo "`cat $i| awk '{print $1"\n"$2}' | grep -ox "[0-9]\+" | sort -u | wc -l`" "  $i"
done
