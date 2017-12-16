#!/usr/bin/env python3

import glob
import json

def main():
    data = []
    for i in glob.glob('results/*.json'):
        with open(i,'r') as j:
            data.append(json.loads(j.read()))


    with open('results.csv', 'w') as f:
        # first print column names
        f.write('name,states,reduction,ace,ce,pe,total,pcap\n')
        names = ['target', 'target states', 'reduction', 'ace',
        'ce','pe','total packets']
        # print json content to csv
        for j in data:
            f.write(','.join((str(j[x]) for x in names)))
            f.write(',' + j['pcaps'][0] + '\n')


if __name__ == "__main__":
    main()
