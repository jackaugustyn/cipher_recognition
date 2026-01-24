cmake_minimum_required(VERSION 3.10)
project(DataEncryptor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Znajdź bibliotekę OpenSSL
find_package(OpenSSL REQUIRED)

# Dodaj pliki wykonywalne
add_executable(encrypt encrypt.cpp)
add_executable(generate_ciphertexts generate_ciphertexts.cpp)

# Połącz z bibliotekami OpenSSL
target_link_libraries(encrypt OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(generate_ciphertexts OpenSSL::SSL OpenSSL::Crypto)

# Kompilacja
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
