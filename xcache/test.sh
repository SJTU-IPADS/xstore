../magic.py config -f build-config2.toml

cmake . ; make;

./test_dispatcher;

./test_logic;

./test_rmi;

./test_sampler;

./test_rmi_t;
