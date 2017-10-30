#!/usr/bin/env python

import os
import argparse
import re
import sys
import subprocess

def ba_to_fa_format(input, output):
    with open(output,'w') as f1:
        with open(input,'r') as f2:
            for line in f2:
                if ',' in line:
                    a,s1,s2 = re.split('(?:,|->)\s*', line[:-1])
                    f1.write(s1[1:-1] + ' ' + s2[1:-1] + ' ' + a + '\n')
                else:
                    f1.write(line[1:-2] + '\n')

def fa_to_ba_format(input, output):
    with open(output,'w') as f1:
        with open(input,'r') as f2:
            for line in f2:
                if ' ' in line:
                    s1, s2, a = line.split()
                    f1.write(a + ',[' + s1 + ']' + '->[' + s2 + ']\n')
                else:
                    f1.write('[' + line[:-1] + ']\n')

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-i','--input', type=str, metavar='FILE',
    help='input file with automaton')
    parser.add_argument('-o','--output', type=str, metavar='FILE',
    help='output file with automaton')
    parser.add_argument('-b','--to-ba', action='store_true', help='converts FA \
    format to BA format, does not reduce the NFA')

    args = parser.parse_args()

    if args.input is None or args.output is None:
        sys.stderr.write('Error: no input/output file specified\n')
        sys.exit(1)

    if args.to_ba:
        fa_to_ba_format(args.input, args.output)
        return

    tmpf1 = "tmpfile1"
    tmpf2 = "tmpfile2"

    fa_to_ba_format(args.input, tmpf1)

    rstr = "java -jar Reduce.jar " + tmpf1 + " 10 -sat -finite -o " + tmpf2
    subprocess.call(rstr.split())
    ba_to_fa_format(tmpf2, args.output)
    os.unlink(tmpf1)
    os.unlink(tmpf2)

main()
