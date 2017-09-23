#!/usr/bin/env python3

from collections import defaultdict
import os, sys
import operator
import argparse

import nnfa
import nnfa_parser

def create_output_name(aut, pcaps, error, depth):
    pass

def get_nnfa_freq(fname, state_map):
    freqs = [0 for x in state_map]
    with open(fname, 'r') as f:
        for line in f:
            state, freq, *_ = line.split()
            freqs[state_map[state]] = int(freq)

    return freqs

def reduce(aut, freqs, *, error=0, depth=0):
    total_freq = max(freqs)
    marked = set()
    sdepth = aut.state_depth
    srt_states = sorted([x for x in range(aut.state_count) \
                        if sdepth[x] >= depth], \
                        key=lambda x : freqs[x])

    for state in srt_states:
        sf = freqs[state]
        err = 0 if sf == 0 else sf / total_freq
        assert (err >= 0 and err <= 1)
        if err <= error:
            marked.add(state)
            error -= err
        else:
            break

    final_state_label = aut.state_count
    new_transitions = [defaultdict(set) for x in range(aut.state_count+1)]
    for state, rules in enumerate(aut._transitions):
        for key, val in rules.items():
            new_transitions[state][key] = \
            set([x if x not in marked else final_state_label for x in val])

    cnt = aut.state_count
    aut._transitions = new_transitions
    aut._final_states.add(final_state_label)
    aut.remove_unreachable()
    sys.stderr.write('{}/{} {:0.2f}%\n'.format(aut.state_count, cnt,
    aut.state_count*100/cnt))

def main():

    parser = argparse.ArgumentParser()
    parser.add_argument('input', type=str, metavar='FILE',
                        help='input file with automaton')
    parser.add_argument('freqs', type=str, metavar='FILE', help='fill \
                        nfa with frequencies')
    parser.add_argument('-o','--output', type=str, metavar='FILE',
                        help='output file, if not specified everything is \
                        printed to stdout')
    parser.add_argument('-s','--add-sl', action='store_true',
                        help='add self-loops to final states')
    # reduction arguments
    parser.add_argument('-e','--max-error',type=float, metavar='ERR', default=0,
                        help='idk')
    parser.add_argument('-d','--depth',type=int, metavar='DEPTH', default=0,
                        help='min depth of pruned state')
    parser.add_argument('-m','--merge-rate',type=float, metavar='N', default=0,
                        help='idk')

    args = parser.parse_args()

    if args.input is None:
        sys.stderr.write('Error: no input specified\n')
        sys.exit(1)

    if args.freqs is None:
        sys.stderr.write('Error: no frequency file specified\n')
        sys.exit(1)

    par = nnfa_parser.NetworkNfaParser()
    a = par.parse_fa(args.input)
    rmap = {val:key for key,val in par._state_map.items()}
    freqs = get_nnfa_freq(args.freqs, par._state_map)
    depth = a.state_depth

    # a little check
    succ = a.succ
    pred = a.pred
    for state, _ in enumerate(a._transitions):
        if len(succ[state]) == 1:
            for x in succ[state]:
                if freqs[x] > freqs[state] and len(pred[x]) == 1:
                    raise RuntimeError('invalid frequencies')

    reduce(a, freqs, error=args.max_error, depth=args.depth)
    if args.add_sl:
        a.selfloop_to_finals()

    if args.output:
        with open(args.output, 'w') as out:
            for line in a.write_fa():
                print(line, file=out)
    else:
        a.print_fa()

if __name__ == "__main__":
    main()
