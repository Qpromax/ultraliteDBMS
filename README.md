## Configuration
~~~bash
conan install . --output-folder=conan_build --build=missing
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=conan_build/conan_toolchain.cmake 
