#!/bin/bash
sudo pwd
mkdir -p third_party/libgo/build
mkdir -p third_party/spdlog/build
mkdir -p third_party/yaml-cpp/build

cd third_party/libgo/build
cmake ..
sudo make install
cd -

cd third_party/spdlog/build
cmake ..
sudo make install
cd -

cd third_party/yaml-cpp/build
cmake ..
sudo make install
cd -