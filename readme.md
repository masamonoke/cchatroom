# About
Console chat application

# Build
```console
git clone https://github.com/masamonoke/cchatroom --recursive
mkdir build && cd build
cmake ..
cmake --build . -j $(sysctl -n hw.logicalcpu)
```

Note that server is running on port 1000

To run server:
```console
./cchatroom-server
```

To run client on same host:
```console
./cchatroom-client "127.0.0.1"
```
