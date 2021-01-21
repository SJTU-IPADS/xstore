# Steps to build XSTORE benchmark & tests

## Prepare and build dependencies

- `./magic.py config -f build-config.toml`;
- Get `PyTorch c++`, check from [PyTorch c++](https://pytorch.org/cppdocs/installing.html);
- `cmake -DCMAKE_PREFIX_PATH=deps=/path/to/pytorch/share/cmake/Torch/ .`;
- Get `Intel Math Kernel Library`, check from [MKL](https://software.intel.com/content/www/us/en/develop/tools/oneapi/components/onemkl.html). Using the default steps for install. Usually, MKL is installed at `/opt/intel/mkl`. If that's not the case, please modify the `mkl = ...` entry in`build-config.toml` and rerun `./magic.py config -f build-config.toml; cmake -DCMAKE_PREFIX_PATH=deps=/path/to/pytorch/share/cmake/Torch/ .`;
- Build `boost` : `make boost` (May take sometime depends on network conditional and computation power);
- Build `jemalloc`: `cd ./deps/jemalloc; autoconf; cd ../../; make jemalloc`;
- `cmake .`

## Build unittests

Most unittests are in separate modules (e.g., x_ml).
For building and running unittests in each module,
you can directly goto each module.

For instance, the following steps will run unittests in `xkv_core`,
which implements the core storage of XStore.

- `cd xkv_core`;
- `./test.sh`

## Build benchmarks

- `make -j12`
