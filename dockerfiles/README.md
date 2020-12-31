# Build a basic image that has RDMA drivers:
- `docker build -t ubuntu-rdma -f ./rdma.Dockerfile .`

# The image that has all MKL's install dependencies:
- `docker build -t ubuntu-mkl -f ./mkl_install.Dockerfile .`
