git submodule update --init --recursive
::python download_assets.py
::python extern/vulkan/download_assets.py
mkdir build
cd build
cmake ../

start LSRViewer.sln