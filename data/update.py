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
            #f.write('reduction,states,ace,ce,pe,total,pcap,')
            f.write('reduction,states,acc1,acc2,cls1,cls2,wrong,correct,')
            f.write('total,pcap,')
            f.write(','.join((k for k in data[0]['reduced rules'].keys())))
            f.write('\n')
            #names = ['reduction', 'reduced states', 'ace', 'ce','pe',
            #    'total packets','pcap']
            names = ['reduction', 'reduced states', 'accepted by target',
                'accepted by reduced', 'target classifications',
                'reduced classifications', 'wrong packet classifications',
                'correct packet classifications', 'total packets','pcap']
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
