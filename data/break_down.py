#!/usr/bin/env python

import matplotlib.pyplot as plt

#data = [5, 20, 15, 25, 10]
#labels = ['Tom', 'Dick', 'Harry', 'Slim', 'Jim']

## performance with optimal
## key_space: 10M
optimal = {
    "sc_rdma_addr" : 114657180.62,
    "sc_rdma"     : 54992063.87,
    "null_rdma_8" : 107350709.86,
    "null_rpc"    : 72241170.57,
    "get_rpc"     :  23695527.78,
}

## performance with bad
## key_space: 10000
not_so_bad = {
    "sc_rdma" : 6532624.60, ## because max page_span = 60
    "null_rdma_384" : 26654064.47,
    "null_rdma_8" : optimal["null_rdma_8"],
    "null_rpc" : optimal["null_rpc"],
    "get_rpc" : 43460522.60,
}

root = optimal
data = [root[k] for k in root.keys()]
labels = [k for k in root.keys() ]

plt.bar(range(len(data)),data,color=['r','g','r'],width=0.4)
plt.xticks(range(len(data)), labels)
plt.show()
