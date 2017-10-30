#!/usr/bin/env python3

from collections import defaultdict
import os, sys
import operator
import argparse
import random

import nnfa
import nnfa_parser
import default_output

def reduce2(aut, *, pct=0, depth=0):
    # result is mapping state->state
    res = {}
    # set of states which will be collapsed
    state_depth = aut.state_depth
    states = [x for x in range(aut.state_count) if state_depth[x] > depth]

    # create equivalence classes
    print(int(aut.state_count * pct) - aut.state_count + len(states));
    equiv_groups_count =int(aut.state_count * pct) - aut.state_count + len(states)
    assert(equiv_groups_count > 1)
    equiv_groups = [set() for x in range(equiv_groups_count)]

    for state in states:
        x = random.randrange(0, equiv_groups_count)
        equiv_groups[x].add(state)

    for eq in equiv_groups:
        if len(eq) > 1:
            p = eq.pop()
            res[p] = p
            for q in eq:
                res[q] = p
        elif len(eq) == 1:
            p = eq.pop()
            res[p] = p
    for key,val in res.items():
        print(key,'->',val)
    return res

def get_nnfa_freq(fname, state_map=None):
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
    parser.add_argument('aut', type=str, metavar='FILE',
                        help='input file with automaton')
    parser.add_argument('-f', '--freqs', type=str, metavar='FILE', help='fill \
                        nfa with frequencies')
    parser.add_argument('-o','--output', type=str, metavar='FILE',
                        help='output file, if not specified everything is \
                        printed to stdout')
    parser.add_argument('-s','--add-sl', action='store_true',
                        help='add self-loops to final states')
    # reduction arguments
    parser.add_argument('-r','--reduction',type=float, metavar='PCT', default=0.5,
                        help='reduction of states')
    parser.add_argument('-e','--max-error',type=float, metavar='ERR', default=0,
                        help='idk')
    parser.add_argument('-d','--depth',type=int, metavar='DEPTH', default=2,
                        help='min depth of pruned state')
    parser.add_argument('-m','--merge-rate',type=float, metavar='N', default=0,
                        help='idk')

    args = parser.parse_args()
    par = nnfa_parser.NetworkNfaParser()
    a = par.parse_fa(args.aut)

    if args.freqs:
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

    reduce2(a, pct=args.reduction, depth=args.depth)
    return
#    reduce(a, freqs, error=args.max_error, depth=args.depth)
    if args.add_sl:
        a.selfloop_to_finals()

#        ofname = args.output
#    else:
#        ofname = default_output.reduced_fname(args.aut, args.freqs,
#                                              args.max_error,args.depth)
#    print('Saving to', ofname)

    if args.output:
        with open(args.output, 'w') as out:
            for line in a.write_fa():
                print(line, file=out)
    else:
        for line in a.write_fa():
            print(line)

if __name__ == "__main__":
    main()
