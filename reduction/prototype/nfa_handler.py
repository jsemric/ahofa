#!/usr/bin/env python3

import argparse
import sys
import nfa

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', type=str, help='input file with NFA')
    parser.add_argument(
        '-d', '--dot', action='store_true', help='output NFA in dot format')
    parser.add_argument(
        '-m', '--minimize', action='store_true', help='light-weight \
        NFA minimization')
    parser.add_argument(
        '-o', '--output', type=str, help='input file with NFA')

    args = parser.parse_args()
    aut = nfa.Nfa.parse_fa(args.input)
    if args.dot:
        gen = aut.to_dot()
    elif args.minimize:
        aut.lightweight_minimization()
        gen = aut.write_fa()
    else:
        return

    if args.output:
        with open(args.output, 'w') as f:
            for i in gen:
                print(i, file=f)
    else:
        for i in gen:
            print(i)


if __name__ == "__main__":
    main()
