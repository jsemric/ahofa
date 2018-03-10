#!/usr/bin/env python3

import glob
import json
import re
import os

folder='data/csv'
exp='data/error/*.json'

def main():
    all_data = []
    duplicates = set()
    for i in glob.glob(exp):
        with open(i,'r') as j:
            error = json.loads(j.read())
            if not (error['reduced'], error['pcap']) in duplicates:
                duplicates.add((error['reduced'], error['pcap']))
                all_data.append(error)

    nfas = set([re.sub('-r.+$','',x['reduced']) for x in all_data])

    # create csv file for each automaton
    for nfa in nfas:
        data = [x for x in all_data if x['reduced'].startswith(nfa)]
        with open(os.path.join(folder, nfa + '.csv'), 'w') as f:
            # first print column names
            f.write('reduction,states,acc1,acc2,cls1,cls2,wrong,correct,')
            f.write('total,pcap,')
            f.write(','.join((k for k in data[0]['reduced rules'].keys())))
            f.write('\n')
            names = ['reduction', 'reduced states', 'accepted target',
                'accepted reduced', 'target classifications',
                'reduced classifications', 'wrong detections',
                'correct detections', 'total packets','pcap']
            # print json content to csv
            for j in data:
                f.write(','.join((str(j[x]) for x in names)))
                f.write(',')
                f.write(
                    ','.join((str(val) for val in j['reduced rules'].values()))
                )
                f.write('\n')

if __name__ == "__main__":
    main()
