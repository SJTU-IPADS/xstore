#### Build

First of all, run `./magic.py config -f build-config.toml` for initialize the dependencies. 
Then, go to each of the directories for specific test/bench executables.

#### YCSB
`./benchs/ycsb` provides an evaluation of YCSB-C workload atop of XStore. 
Please check `./scripts/benchs/ycsb_build.toml` for how to build it; 
and `./scripts/benchs/ycsb_run.toml` for how to run it.
