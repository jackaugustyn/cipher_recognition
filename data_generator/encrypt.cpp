#include <iostream>
#include <vector>
#include <random>
#include <iomanip>
#include <cstring>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/des.h>
#include <openssl/blowfish.h>
#include <openssl/cast.h>
#include <openssl/rc4.h>

// Wyciszenie ostrzeżeń o przestarzałych funkcjach OpenSSL (funkcje nadal działają)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

class DataEncryptor {
private:
    std::mt19937 generator;
    std::vector<unsigned char> randomData;
    unsigned char key56[7]; // 56 bits = 7 bytes
    
    void generateRandomData(size_t size) {
        randomData.resize(size);
        std::uniform_int_distribution<unsigned char> dist(0, 255);
        for (size_t i = 0; i < size; i++) {
            randomData[i] = dist(generator);
        }
    }
    
    void generate56BitKey() {
        std::uniform_int_distribution<unsigned char> dist(0, 255);
        for (int i = 0; i < 7; i++) {
            key56[i] = dist(generator);
        }
    }
    
    void printHex(const std::vector<unsigned char>& data, const std::string& label) {
        std::cout << label << ": ";
        for (size_t i = 0; i < data.size(); i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(data[i]) << " ";
        }
        std::cout << std::dec << std::endl;
    }

public:
    DataEncryptor(unsigned int seed) : generator(seed) {
        generate56BitKey();
    }
    
    void generateData(size_t size) {
        generateRandomData(size);
        std::cout << "\n=== Wygenerowane dane losowe ===" << std::endl;
        printHex(randomData, "Dane");
        printHex(std::vector<unsigned char>(key56, key56 + 7), "Klucz 56-bit");
    }
    
    std::vector<unsigned char> encryptCAST() {
        std::vector<unsigned char> encrypted(randomData.size());
        CAST_KEY castKey;
        
        // CAST przyjmuje klucz do 128 bitów, użyjemy 56 bitów
        CAST_set_key(&castKey, 7, key56);
        
        unsigned char iv[CAST_BLOCK];
        memset(iv, 0, CAST_BLOCK);
        
        // CAST w trybie ECB (prostszy do implementacji)
        for (size_t i = 0; i < randomData.size(); i += CAST_BLOCK) {
            size_t blockSize = std::min(static_cast<size_t>(CAST_BLOCK), 
                                       randomData.size() - i);
            unsigned char input[CAST_BLOCK] = {0};
            unsigned char output[CAST_BLOCK];
            
            memcpy(input, &randomData[i], blockSize);
            CAST_ecb_encrypt(input, output, &castKey, CAST_ENCRYPT);
            memcpy(&encrypted[i], output, blockSize);
        }
        
        return encrypted;
    }
    
    std::vector<unsigned char> encryptRC4() {
        std::vector<unsigned char> encrypted(randomData.size());
        RC4_KEY rc4Key;
        
        RC4_set_key(&rc4Key, 7, key56);
        RC4(&rc4Key, randomData.size(), randomData.data(), encrypted.data());
        
        return encrypted;
    }
    
    std::vector<unsigned char> encryptDES() {
        std::vector<unsigned char> encrypted(randomData.size());
        DES_key_schedule desKey;
        
        // DES używa 56-bitowego klucza (7 bajtów, ale potrzebuje 8 bajtów)
        unsigned char desKey8[8];
        memcpy(desKey8, key56, 7);
        desKey8[7] = 0; // Uzupełnienie do 8 bajtów
        
        // Ustawienie parzystości bitów klucza (wymagane przez DES)
        DES_set_odd_parity(reinterpret_cast<DES_cblock*>(desKey8));
        DES_set_key_unchecked(reinterpret_cast<const_DES_cblock*>(desKey8), &desKey);
        
        unsigned char iv[8];
        memset(iv, 0, 8);
        
        // DES w trybie ECB
        for (size_t i = 0; i < randomData.size(); i += 8) {
            size_t blockSize = std::min(static_cast<size_t>(8), 
                                       randomData.size() - i);
            unsigned char input[8] = {0};
            unsigned char output[8];
            
            memcpy(input, &randomData[i], blockSize);
            DES_ecb_encrypt(reinterpret_cast<const_DES_cblock*>(input),
                          reinterpret_cast<DES_cblock*>(output),
                          &desKey, DES_ENCRYPT);
            memcpy(&encrypted[i], output, blockSize);
        }
        
        return encrypted;
    }
    
    std::vector<unsigned char> encryptBlowfish() {
        std::vector<unsigned char> encrypted(randomData.size());
        BF_KEY bfKey;
        
        // Blowfish może używać kluczy od 32 do 448 bitów
        BF_set_key(&bfKey, 7, key56);
        
        unsigned char iv[BF_BLOCK];
        memset(iv, 0, BF_BLOCK);
        
        // Blowfish w trybie ECB
        for (size_t i = 0; i < randomData.size(); i += BF_BLOCK) {
            size_t blockSize = std::min(static_cast<size_t>(BF_BLOCK), 
                                       randomData.size() - i);
            unsigned char input[BF_BLOCK] = {0};
            unsigned char output[BF_BLOCK];
            
            memcpy(input, &randomData[i], blockSize);
            BF_ecb_encrypt(input, output, &bfKey, BF_ENCRYPT);
            memcpy(&encrypted[i], output, blockSize);
        }
        
        return encrypted;
    }
    
    void runEncryption(size_t dataSize) {
        generateData(dataSize);
        
        std::cout << "\n=== Szyfrowanie algorytmem CAST ===" << std::endl;
        auto castEncrypted = encryptCAST();
        printHex(castEncrypted, "Zaszyfrowane");
        
        std::cout << "\n=== Szyfrowanie algorytmem RC4 ===" << std::endl;
        auto rc4Encrypted = encryptRC4();
        printHex(rc4Encrypted, "Zaszyfrowane");
        
        std::cout << "\n=== Szyfrowanie algorytmem DES ===" << std::endl;
        auto desEncrypted = encryptDES();
        printHex(desEncrypted, "Zaszyfrowane");
        
        std::cout << "\n=== Szyfrowanie algorytmem Blowfish ===" << std::endl;
        auto blowfishEncrypted = encryptBlowfish();
        printHex(blowfishEncrypted, "Zaszyfrowane");
    }
};

int main(int argc, char* argv[]) {
    unsigned int seed = 12345;
    size_t dataSize = 64; // Domyślny rozmiar danych w bajtach
    
    if (argc > 1) {
        seed = std::stoul(argv[1]);
    }
    if (argc > 2) {
        dataSize = std::stoul(argv[2]);
    }
    
    std::cout << "Program szyfrujący dane losowe" << std::endl;
    std::cout << "Ziarno generatora: " << seed << std::endl;
    std::cout << "Rozmiar danych: " << dataSize << " bajtów" << std::endl;
    
    DataEncryptor encryptor(seed);
    encryptor.runEncryption(dataSize);
    
    return 0;
}

#pragma GCC diagnostic pop
