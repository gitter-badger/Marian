Marian
======
A C++ gpu-specific parallel automatic differentiation library
with operator overloading.

In honour of Marian Rejewski, a Polish mathematician and
cryptologist.

Installation
------------

Requirements:

* g++ with c++11
* CUDA and CuDNN
* Boost (>= 1.56)

Exporting some paths for CuDNN may be required (put it, for example, in your `.bashrc` file):

    export PATH=$PATH:$HOME/.local/bin:/usr/local/cuda/bin
    export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/cuda/lib64:/usr/local/cuda/lib64/stubs:/usr/local/cudnn-5/lib64
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/cuda/lib64:/usr/local/cuda/lib64/stubs:/usr/local/cudnn-5/lib64
    export CPATH=$CPATH:/usr/local/cudnn-5/include

Compilation with `cmake > 3.5`:

    mkdir build
    cd build
    cmake ..
    make -j

To compile API documentation using Doxygen, first cd to the build directory, and then:

    make doc
