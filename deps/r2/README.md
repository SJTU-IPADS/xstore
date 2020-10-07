### Run example of simple one-sided RDMA

`cd /path/to/r2/; cmake .; make oserver;make oclient;`

Suppose the server is at 1.1.1.1, so first at server, run: `./oserver`;

Then at client run `./oclient --server_host=1.1.1.1`; Then all is done. 

The codes are in `./examples/basic_onesided_rdma`. 