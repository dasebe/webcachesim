* Recommend OS: Ubuntu 18
```bash

sudo apt-get update

sudo apt install -y git cmake build-essential libboost-all-dev python3-pip parallel libprocps-dev

# install openjdk 1.8. This is for simulating Adaptive-TinyLFU. The steps has to be one by one
sudo add-apt-repository ppa:openjdk-r/ppa
sudo apt-get update
sudo apt-get install -y openjdk-8-jdk
java -version  
# should be 1.8


git clone https://github.com/sunnyszy/lrb webcachesim

# dependency can be optionally installed in non-default path using CMAKE_INSTALL_PREFIX CMAKE_PREFIX_PATH
cd webcachesim

# install LightGBM
cd lib/LightGBM-eloiseh/build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j8
sudo make install
cd ../../..

# dependency for mongo c driver
sudo apt-get install -y cmake libssl-dev libsasl2-dev

# installing mongo c
cd lib/mongo-c-driver-1.13.1/cmake-build/
cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF ..
make -j8
sudo make install
cd ../../..

# installing mongo-cxx
cd lib/mongo-cxx-driver-r3.4.0/build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
sudo make -j8
sudo make install
cd ../../..

# installing libbf
cd lib/libbf-dadd48e/build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j8
sudo make install
cd ../../..


# building webcachesim, install the library with api
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j8
sudo make install
sudo ldconfig
cd ..

# add binary and trace dir to path
export PATH=$PATH:${YOUR webcachesim DIR}/build/bin
export WEBCACHESIM_TRACE_DIR=${YOUR TRACE DIR}
export WEBCACHESIM_ROOT=${YOUR webcachesim DIR}

webcachesim_cli
#output: webcachesim_cli traceFile cacheType cacheSize [--param=value] 

# install python framework
pip3 install -e .


```
* [Optional] Set up a mongodb instance to save simulation results as json file to mongodb. One option is using [mlab](https://mlab.com/home) to register a free one.
* [Optional] pywebcachesim is a python wrapper to run multiple simulation in parallel on multiple nodes.
```shell script
python3 pywebcachesim/runner/runner.py --config_file ${YOUR CONFIG FILE} --algorithm_param_file ${YOUR ALGORITHM PARAM FILE} --trace_param_file ${YOUR TRACE PARAM FILE} --authentication_file ${YOUR AUTHENTICATION FILE} 
```
