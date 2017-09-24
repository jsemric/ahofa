#!/usr/bin/env python3

import argparse
import subprocess
from concurrent import futures
import os

import default_output

def compute_error(args):
    aut1, aut2, pcap = args
    prog = ' '.join(['./nfa_handler -t', aut1, '-a', aut2, '-p', pcap])
    try:
        result = subprocess.check_output(prog.split())
    except OSError as e:
        print(e)

    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('target',help='input NFA',metavar='AUT1')
    ap.add_argument('aut',help='input NFA which supposed to be an \
                    overapproximation of the previous NFA',metavar='AUT2')
    ap.add_argument('-p','--pcaps',nargs='*', help='input pcap files',
                    metavar='PCAP')
    ap.add_argument('-o','--output',type=str,metavar='FILE',help='output file')
    ap.add_argument('-w','--nworkers',type=int,help='workers to run in \
                    parallel', default=2)
    args = ap.parse_args()

    cargs = [(args.target, args.aut, x) for x in args.pcaps]
    with futures.ProcessPoolExecutor(args.nworkers) as pool:
        results = pool.map(compute_error, cargs)

    total, acc1, acc2 = 0, 0, 0
    for r in results:
        t, a1, a2 = r.split()
        total += int(t)
        acc1 += int(a1)
        acc2 += int(a2)

    out = 'Total: {}\nAccepted1: {}\nAccepted2: {}\n'.format(total, acc1, acc2)
    out += 'Error: {:0.10f}'.format((acc2-acc1)/total)
    if args.output is None:
        ofname = default_output.error_fname(args.target, args.aut, *args.pcaps)
        print('Saving to', ofname)
    else:
        ofname = args.output

    with open(ofname, 'w') as f:
        f.write(out)

    print(out, end='')

if __name__ == "__main__":
    main()
