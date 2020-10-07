#!/usr/bin/env python

import numpy as np
import importlib
import matplotlib.pyplot as plt
from pylab import *

from common import *

from sys import *

def plot_all(data,legends):
    ax = plt.subplot(1, 1, 1)
    x = range(len(data[0]))
    #print(data)
    for d in data:
        #print(d)
        plt.plot(x,d)
    ax.legend(legends)
    pass

def main():

    plt.subplots_adjust(left=0.14,bottom=0.2, right=0.67, top=0.65)
    plt.tick_params(axis='both', which='major', labelsize=10)

    module = "ycsbd_refine"

    if len(sys.argv) > 1:
        module = str(sys.argv[1])
    print("plot using module", module)

    plt.tick_params(axis='both', which='major', labelsize=10)
    plt.figure(num=1, figsize=(8 * (0.14 + 0.2), 5 * (0.67 + 0.65)),) ## 8,5: width, height

    res = importlib.import_module(module)
    plot_all(res.data,res.legends)

    bottom, top = ylim()
    try:
        top = res.ylim
    except:
        pass
    ylim(0,top)
    plt.ylabel(res.ylabel,fontsize = 14)
    plt.xlabel('time (seconds)',fontsize = 12)

    foo_fig = plt.gcf() # 'get current figure'
    foo_fig.savefig(module + '.eps', format='eps', dpi=1000)

    plt.show()

if __name__ == "__main__":
    main()
