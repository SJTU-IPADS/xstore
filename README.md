# XStore: an RDMA-based Ordered Key-Value Store

XStore is an RDMA-enabled **ordered** key-value store targeted at a client-sever setting. Specifically, the server has an in-memory B+Tree; and the client uses one-sided RDMA READs to traverse the B+Tree. To accelerate the lookup, we deploy a learned cache (xcache) at the client to accomplish the traversal in one round-trip (if the learned cache is all cached). Even if the xcahe is not cached, XStore client only needs at most 2 round-trips for the traversal. 


## Feature Highlights

* High-performance & scalable in-memory ordered key-value store
* High-performance learned index with a two-layer RMI structure as a cache
* Various machine learning models pre-built
* Fast communication by leveraging RDMA feature of InfiniBand network

## Features uncovered

This codebase has the basic functionality of XStore, including basic XCache training, deployment, and usage in a **static** workload. 
Other features will be released soon.

## Getting Started

TODO

## License
XStore uses [SATA License](LICENSE.txt) (Star And Thank Author License, originally [here](https://github.com/zTrix/sata-license)) :)

If you use XStore in your research, please cite our paper:
   
    @inproceedings {xstore2020,
        author = {Xingda Wei and Rong Chen and Haibo Chen},
        title = {Fast RDMA-based Ordered Key-Value Store using Remote Learned Cache},
        booktitle = {14th {USENIX} Symposium on Operating Systems Design and Implementation ({OSDI} 20)},
        year = {2020},
        isbn = {978-1-939133-19-9},
        pages = {117--135},
        url = {https://www.usenix.org/conference/osdi20/presentation/wei},
        publisher = {{USENIX} Association},
        month = nov,
    }


## Academic and Reference Papers

[**OSDI**] [Fast RDMA-based Ordered Key-Value Store using Remote Learned Cache](docs/papers/xstore-osdi20.pdf). Xingda Wei and Rong Chen and Haibo Chen. Proceedings of 14th USENIX Symposium on Operating Systems Design and Implementation, Nov, 2020. 
