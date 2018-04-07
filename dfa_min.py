#!/usr/bin/env python3

import sys, os
import re
import argparse
import tempfile

from nfa import Nfa
sys.path.insert(0,"./symboliclib")
import symboliclib as sbl

def write_output(fname, gen):
    with open(fname, 'w') as f:
        for i in gen:
            f.write(i)

def fa2timbuk(nfa, mapping, fname):
    with open(fname,'w') as f:
        f.write('Automaton A @LFA\n')
        f.write('Ops x:0 ' + ' '.join(str(i) + ':1' for i in range(256)))
        f.write(' ' + ' '.join(str(x) + ':1' for x in mapping.keys()) + '\n')
        f.write('States ' + ' '.join('q'+str(x) for x in nfa.states) + '\n')
        f.write('Final States ' +
            ' '.join('q'+str(x) for x in nfa._final_states) + '\n')
        f.write('Transitions\n')
        f.write('x->q' + str(nfa._initial_state) + '\n')

        for state, rules in nfa._transitions.items():
            for sym, states in rules.items():
                for q in states:
                    f.write('{}(q{}) -> q{}\n'.format(sym,state,q))


def timbuk2fa(fname):
    nfa = Nfa()
    smap = dict()
    cnt = 0

    def add_state(p):
        nonlocal smap
        nonlocal cnt
        if not p in smap:
            smap[p] = cnt
            cnt += 1
        return smap[p]


    with open(fname,'r') as f:
        ts = 0
        for line in f:
            line = line[:-1]
            if line == "": continue
            if line.startswith("Final States"):
                for x in [x for x in line.split()][2:]:
                    nfa._add_final_state(add_state(x))

            if ts == 1:
                init = line.split()[-1]
                nfa._add_initial_state(add_state(init))
                ts = 2
            elif ts == 2:
                a, p, q = re.sub('["\(\)\->]',' ',line).split()
                p = add_state(p)
                q = add_state(q)
                nfa._add_rule(p,q,int(a))

            if line.startswith("Transitions"):
                ts = 1
    return nfa


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', metavar='NFA', type=str)
    parser.add_argument('output', metavar='OUT', type=str)
    args = parser.parse_args()

    tb_file = tempfile.NamedTemporaryFile()
    min_file = tempfile.NamedTemporaryFile()

    sys.stderr.write('Parsing FA file\n')
    aut = Nfa.parse(args.input)
    sys.stderr.write('Extending final states\n')
    mapping = aut.extend_final_states()
    sys.stderr.write('Converting to timbuk format\n')
    fa2timbuk(aut, mapping, tb_file.name)
    sys.stderr.write('Parsing timbuk file\n')
    a = sbl.parse(tb_file.name)
    sys.stderr.write('Minimizing\n')
    a.minimize().print_automaton(min_file.name)
    sys.stderr.write('Converting to FA\n')
    aut = timbuk2fa(min_file.name)
    sys.stderr.write('Retrieving final states\n')
    aut.retrieve_final_states()
    
    write_output(args.output, aut.write())
    sys.stderr.write('Saved as ' + args.output + '\n')


main()
