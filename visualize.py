#!/usr/bin/env python3

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

def plot_errors(fname):
    df = pd.read_csv(fname)
    df = df.drop_duplicates()
    # aggregate all values for each reduction
    df_aggr = df.groupby(['reduction','states'])[['acc1','acc2','cls1','cls2','wrong','correct','total']].sum()
    # compute other statistics
    df_aggr['ce'] = (df_aggr['cls2'] - df_aggr['cls1']) / df_aggr['total']
    # plot data
    #df_aggr.sort_index()
    df_aggr.plot(y='ce', marker='o')
    plt.show()

def main():
    plot_errors('data/sprobe.csv')
#    pcap_analysis()

def pcap_analysis(fname='mc.txt'):
    # read with mixed data type, numpy stores result in 1d structured array
    data = np.log(np.loadtxt(open(fname,'r'), delimiter=' '))
    plt.imshow(data, cmap='hot', interpolation='nearest')
    plt.show()

if __name__ == "__main__":
    main()
