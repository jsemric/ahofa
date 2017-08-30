#!/usr/bin/env python3

import argparse
import subprocess
from multiprocessing import Process
import glob
import os
import tempfile

def compute_error():
    out = None
    pcap = None
    aut1 = None
    aut2 = None

    def set_output(f):
        nonlocal out
        out = f

    def set_pcap(f):
        nonlocal pcap
        pcap = f

    def set_nfas(a1, a2):
        nonlocal aut1
        nonlocal aut2
        aut1, aut2 = a1, a2

    def get_output_file():
        return out

    def ce_inst():
        prog = ' '.join(['./nfa_handler -t',aut1,'-a',aut2,'-p',pcap])
        subprocess.run(prog.split(),stdout=out)

    ce_inst.set_output = set_output
    ce_inst.set_pcap = set_pcap
    ce_inst.set_nfas = set_nfas
    ce_inst.get_output_file = get_output_file

    return ce_inst


def create_err_func(aut1, aut2, pcap):
    f = compute_error()
    f.set_output(tempfile.TemporaryFile())
    f.set_pcap(pcap)
    f.set_nfas(aut1, aut2)
    return f

def run_in_parallel(*fns):
    proc = []
    for fn in fns:
        p = Process(target=fn)
        p.start()
        proc.append(p)
    for p in proc:
        p.join()
'''
def run_in_parallel(*fns):
    for fn in fns:
        fn()
'''
def parallel_proc(fns, cores):
    flist = []
    for i,f in enumerate(fns):
        flist.append(f)
        if i % cores == cores - 1:
            run_in_parallel(*flist)
            flist = []
    if flist:
        run_in_parallel(*flist)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('target',help='input NFA',metavar='AUT1')
    ap.add_argument('aut',help='input NFA which supposed to be an \
                    overapproximation of the previous NFA',metavar='AUT2')
    ap.add_argument('-p','--pcaps',help='input pcap files',metavar='PCAP')
    ap.add_argument('-o','--output',type=str,metavar='FILE',help='output file')
    ap.add_argument('-c','--cores',type=int,default=1,
                    help='number of cores to run')
    args = ap.parse_args()

    pfns = [create_err_func(args.target,args.aut,f) \
            for f in glob.glob(args.pcaps)]
    parallel_proc(pfns, args.cores)

    out=None
    if args.output:
        out = open(args.output,'w')

    for f in [func.get_output_file() for func in pfns]:
        f.seek(0)
        print(f.read(),file=out)
        f.close()

    if args.output:
        out.close()

if __name__ == "__main__":
    main()
