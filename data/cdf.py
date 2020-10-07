#!/usr/bin/env python

import numpy as np
import importlib
import argparse

from pylab import *
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

sample_gap = 100
max_points = 10000

def sample_show():
    # Create some test data
    dx = 0.01
    X  = np.arange(-2, 2, dx)
    Y  = exp(-X ** 2)

    # Normalize the data to a proper PDF
    Y /= (dx * Y).sum()

    # Compute the CDF
    CY = np.cumsum(Y * dx)

    # Plot both
    plot(X, Y)
    plot(X, CY, 'r--')

    show()

def plot_simple(X,Y, max):
    assert(False)
    l = plot(X[0:max], Y[0:max])
    legend(handles=l)
    show()
    #plt.savefig('testplot.png')

def threshold_data(data,num):
    res = []
    for i in range(0,min(len(data),num)):
        res.append(data[i])
    return res

def sample_data(data,gap):
    if ((len(data) / float(gap)) < 100):
        return data
    res = []
    for i in range(0,len(data),gap):
        res.append(data[i])
    return res

def cut_data(data,max):
    res = []
    for i in range(max):
        if i < len(data):
            res.append(data[i])
    return res

def plot_in_one(l,labels, max):
    ax = plt.subplot(1, 1, 1)

    for d in l:
        plt.plot(cut_data(d.X,max), cut_data(d.Y,max))
    ax.legend(labels)
    plt.show()
    pass


def plot_multi(l, max):
    lines = []

    # plot seperately
    for idx,i in enumerate(l):
        ax = plt.subplot(len(l) + 1,1,idx + 1)
        limit = len(i.X) - 1
#        xlim(i.X[0],i.X[limit])
        #ylim(0,max(i.Y) + 2)
        #ax.xaxis.set_major_locator(ticker.MultipleLocator(sample_gap0))
        sample_gap = int(len(i.Y) / max_points)
        ll = plot(cut_data(i.X, max), cut_data(i.Y, max))
        #ll = plot(sample_data(i.X,sample_gap),sample_data(i.Y,sample_gap),"-",label = i.title)
        #ll = plot(threshold_data(i.X,1000),threshold_data(i.Y,1000),"-",label = i.title)
        xlabel(i.xlabel)
        ylabel(i.ylabel)
        legend(handles = ll)

    ## plot all in one
    """
    plt.subplot(len(l) + 1, 1, len(l) + 1)
    for i in l:
        x = sample_data(i.X,sample_gap)
        y = sample_data(i.Y,sample_gap)
        lines.append((plot(x,y,'.',label = i.title))[0])
    legend(handles = lines)
    """
    show()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--data',default="data",nargs="+")
    parser.add_argument('-s', '--sample',default=10000)
    parser.add_argument('-m', '--max',default=100,type=int)

    args = parser.parse_args()

    global sample_gap
    sample_gap = int(args.sample)

    if (type(args.data) is list) and True:
        datas = [importlib.import_module(i) for i in args.data]
        labels = args.data
        #plot_multi(datas, args.max)
        plot_in_one(datas, labels, args.max)
    else:
        data = importlib.import_module(args.data)
        plot_simple(data.X,data.Y,args.max)

if __name__ == "__main__":
    main()
