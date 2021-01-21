# XStore: an RDMA-based Ordered Key-Value Store

XStore is an RDMA-enabled **ordered** key-value store targeted at a client-sever setting. Specifically, the server has an in-memory B+Tree; and the client uses one-sided RDMA READs to traverse the B+Tree. To accelerate the lookup, we deploy a learned cache (xcache) at the client to accomplish the traversal in one round-trip (if the learned cache is all cached). Even if the xcahe is not cached, XStore client only needs at most 2 round-trips for the traversal.

## Feature Highlights

- High-performance & scalable in-memory ordered key-value store
- High-performance learned index with a two-layer RMI structure as a cache
- Various machine learning models pre-built
- Fast communication by leveraging RDMA feature of InfiniBand network

## Features not supported yet

This codebase has the basic functionality of XStore, including basic XCache training, deployment, various learned models.
It also provides various benchmark code for XCache analysis and static benchmark code.
Other features will be released soon.

## Getting Started

XStore is a **header-only** library.
Basically, `git clone https://github.com/SJTU-IPADS/xstore.git xstore-open --recursive` is all you need to use XStore.
To use XStore, just include the required header to your project.
For instance, ` xkv_core/src/xtree/mod.hh` provides the implementation of `XTree`.

XStore also provides various benchmark and unit tests code.
To build these, please check the following documents:

- [Build](docs/build.md)

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
