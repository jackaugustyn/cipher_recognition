cmake_minimum_required(VERSION 3.10)
project(DataEncryptor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Znajdź bibliotekę OpenSSL i zlib
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# Dodaj pliki wykonywalne
add_executable(encrypt encrypt.cpp)
add_executable(generate_ciphertexts generate_ciphertexts.cpp)
add_executable(generate_text generate_text.cpp text_generator.cpp)
add_executable(generate_encrypted_text generate_encrypted_text.cpp text_generator.cpp)
add_executable(generate_fake_text_ciphertexts generate_fake_text_ciphertexts.cpp text_generator.cpp)
add_executable(generate_compressed_text generate_compressed_text.cpp text_generator.cpp)

# Połącz z bibliotekami OpenSSL / zlib
target_link_libraries(encrypt OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(generate_ciphertexts OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(generate_encrypted_text OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(generate_fake_text_ciphertexts OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(generate_compressed_text ZLIB::ZLIB)

# Kompilacja
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
