# Description #

Volume rendering system to visualize eye tracking data of people watching dynamic stimuli.
For more detail check out the paper: https://vbruder.github.io/publication/dblp-confetra-bruder-kfwe-19/dblp-confetra-bruder-kfwe-19.pdf

# Setup and build #

To compile the code you need:

* An OpenCL 1.2 capable device and drivers/libraries with image support. It is recommended to update your GPU driver before building/running.
* Qt version 5.6 or higher.
* A compiler capable of c++14.
* CMake version 3.9 or higher.

Use CMake to build the volume rasycaster:
```
git clone https://theVall@bitbucket.org/theVall/basicvolumeraycaster.git
cd basicvolumeraycaster
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/install
make -j `nproc`
```
Make sure to correctly setup the path to your Qt install directory.
E.g., replace the CMAKE_PREFIX_PATH with ```/home/username/Qt/5.11.2/gcc_64/```

# Confirmed to build/run on the following configurations #

* NVIDIA Maxwell & Pascal, AMD Fiji & Vega, Intel Gen9 GPU & Skylake CPU
* GCC 5.3.1 & 7.3.0, Visual Studio 2015 (v140), Clang 6.0
* Qt 5.11.2
* CMake 3.10.2 & 3.12.2

# Screenshots #

![app](https://github.com/vbruder/EyeTrackingVolume/blob/master/screenshots/app.png)

![uno-magnitude](https://github.com/vbruder/EyeTrackingVolume/blob/master/screenshots/unomag.png)

# License #

Copyright (C) 2017-2019 Valentin Bruder vbruder@gmail.com

This software is licensed under [LGPLv3+](https://www.gnu.org/licenses/lgpl-3.0.en.html).

# Credits #
	
  * Color wheel from Mattia Basaglia's Qt-Color-Widgets: https://github.com/mbasaglia/Qt-Color-Widgets
  * OpenCL utils based on Erik Smistad's OpenCLUtilityLibrary: https://github.com/smistad/OpenCLUtilityLibrary
  * Transfer function editor based on Qt sample code.
