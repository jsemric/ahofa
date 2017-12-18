#!/usr/bin/env python3

import glob
import json

def main():
    all_data = []
    for i in glob.glob('results/*.json'):
        with open(i,'r') as j:
            all_data.append(json.loads(j.read()))


    nfas = set([x['target'] for x in all_data])
    # create csv file for each automaton
    for nfa in nfas:
        data = [x for x in all_data if x['target'] == nfa]
        with open(nfa + '.csv', 'w') as f:
            # first print column names
            f.write('reduction,states,ace,ce,pe,total,')
            f.write(','.join((k for k in data[0]['reduced rules'].keys())))
            f.write(',pcap\n')
            names = ['reduction', 'reduced states', 'ace', 'ce','pe',
                'total packets']
            # print json content to csv
            for j in data:
                f.write(','.join((str(j[x]) for x in names)))
                f.write(',')
                f.write(','.join((str(val) for val in j['reduced rules'].values())))
                f.write(',' + j['pcaps'][0] + '\n')


if __name__ == "__main__":
    main()
