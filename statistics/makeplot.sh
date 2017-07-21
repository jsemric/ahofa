#!/bin/bash

INPUT=""
OUTPUT="output"
TMPFILE="tmptmptmptmptmp"

while getopts "i:o:h" opt;do
    case $opt in
        i)
        INPUT="$OPTARG";;
        o)
        OUTPUT="$OPTARG";;
        h)
        echo "Usage: $0 -i FILE [-o FILE]"
        exit 0;;
        \?)
        exit 1;;
    esac
done

if [ -z "$INPUT" ];then
    echo "Error: input argument required." >&2
    exit 1
fi

./sum_freqs.py -i $INPUT -d -l -o $TMPFILE
cat $TMPFILE | sort -k3 -n | awk '{print FNR,$2" "$3}' > "$OUTPUT.data"
gnuplot -e "filename='$OUTPUT.data'" -e "out='$OUTPUT.png'" nfa_stats.plg
rm -f $TMPFILE
