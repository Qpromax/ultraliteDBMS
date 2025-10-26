## Configuration
~~~bash
conan profile detect

conan install . --output-folder=conan_build --build=missing

cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=conan_build/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
