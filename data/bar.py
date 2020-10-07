#!/usr/bin/env python

import matplotlib.pyplot as plt

#data = [5, 20, 15, 25, 10]
#labels = ['Tom', 'Dick', 'Harry', 'Slim', 'Jim']

## performance with optimal
## key_space: 10M
optimal = {
    "predict + fetch idx" : 114657180.62,
    "+fetch_value"     : 54992063.87,
    "null_rdma_8" : 107350709.86,
    "null_rpc"    : 72241170.57,
    "get_rpc"     :  23695527.78,
}

## performance with bad
## key_space: 10000
not_so_bad = {
    "predict + idx + value" : 6532624.60, ## because max page_span = 60
    "null_rdma_384" : 26654064.47,
    "null_rdma_8" : optimal["null_rdma_8"],
    "null_rpc" : optimal["null_rpc"],
    "get_rpc" : 43460522.60,
}

lat_breakdown = {
    "raw RDMA" : 5.12188,
    "raw RDMA + ML" : 5.54533,
    "XCache-shared" : 6.32475,
    "XCached-per-client" : 6.66,
    "Tree-per-client" : 7.3481
}

#root = optimal
#root = not_so_bad
root = lat_breakdown
data = [root[k] for k in root.keys()]
labels = [k for k in root.keys() ]

bars = plt.bar(range(len(data)),data,
               color=['#B2182B','#D6604D','#FDDBC7','#4393C3','#D1E5F0'],
               width=0.4,edgecolor='black',linewidth=1)
#for bar in bars:
    #bar.set_linewidth(1)
plt.xticks(range(len(data)), labels)
plt.xticks(rotation=15)
plt.show()
