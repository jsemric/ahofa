#!/usr/bin/env python3

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from scipy.stats import zscore

def print_outliers(df):
    # compute zscore
    df['ce'] = (df['cls2'] - df['cls1']) / df['total']
    df_gr = df.groupby(['reduction','states'])[['ce']].transform(zscore)
    outliers = df_gr['ce'] > 3
    print(df.loc[outliers,['ce','pcap']])

def plot_errors(fname):
    df = pd.read_csv(fname)
    df = df.drop_duplicates()
    # aggregate all values for each reduction
    df_aggr = df.groupby('reduction')[
        ['acc1','acc2','cls1','cls2','wrong','correct','total']].sum()
    # compute other statistics
    df_aggr['error'] = (df_aggr['wrong'] / df_aggr['total'])

    # plotting the results
    plt.style.use('ggplot')
    # displaying only values in reasonable ranges
    filt = (df_aggr['error'] < 0.3) & (df_aggr['error'] > 0.00005)
    df_aggr[filt].plot(y='error', marker='o')
    # setting axis ranges
    plt.xlim(0.12,0.32)
    plt.ylim(-0.01,0.27)
    #plt.grid()
    plt.title(
        'sprobe error (' + format(int(df_aggr.iloc[0]['total']),',') + ')')
    plt.xlabel('reduction ratio')
    plt.ylabel('error')
    plt.show()

def main():
#    plot_errors('data/sprobe.csv')
    pcap_analysis()

def pcap_analysis(fname='mc.txt'):
    # read with mixed data type, numpy stores result in 1d structured array
    data = np.log10(np.loadtxt(open(fname,'r'), delimiter=' '))
    close(filename)
    plt.imshow(data, cmap='jet', interpolation='nearest')
    plt.colorbar()
    plt.show()

if __name__ == "__main__":
    main()
