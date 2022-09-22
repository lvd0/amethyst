# amethyst
small toy renderer

how to compile:
windows:
    git clone --recursive https://github.com/lvd0/amethyst.git
    cd amethyst
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
    ninja -jthreads
    .\test_visbuffer.exe

linux:
    git clone --recursive https://github.com/lvd0/amethyst.git
    cd amethyst
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
    ninja -jthreads
    ./test_visbuffer
