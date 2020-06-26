../magic.py config -f build-config.toml

cmake . ; make;

./test_dispatcher
./test_logic
./test_rmi
./test_sampler
