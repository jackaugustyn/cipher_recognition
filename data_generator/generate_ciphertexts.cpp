#include <iostream>
#include <vector>
#include <random>
#include <iomanip>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/des.h>
#include <openssl/blowfish.h>
#include <openssl/cast.h>
#include <openssl/rc4.h>

// Wyciszenie ostrzeżeń o przestarzałych funkcjach OpenSSL
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

class CiphertextGenerator {
private:
    std::mt19937 generator;
    unsigned char key56[7]; // 56 bits = 7 bytes
    const size_t FILE_SIZE_GB = 8;
    const size_t FILE_SIZE_BYTES = FILE_SIZE_GB * 1024ULL * 1024ULL * 1024ULL;
    const size_t CHUNK_SIZE = 100 * 1024 * 1024; // 100 MB chunków dla efektywnego przetwarzania
    
    void generate56BitKey(unsigned int seed) {
        std::mt19937 keyGen(seed);
        std::uniform_int_distribution<unsigned char> dist(0, 255);
        for (int i = 0; i < 7; i++) {
            key56[i] = dist(keyGen);
        }
    }
    
    void generateRandomData(std::vector<unsigned char>& data, size_t size, unsigned int seed) {
        data.resize(size);
        std::mt19937 localGen(seed);
        std::uniform_int_distribution<unsigned char> dist(0, 255);
        for (size_t i = 0; i < size; i++) {
            data[i] = dist(localGen);
        }
    }
    
    std::vector<unsigned char> encryptCAST(const std::vector<unsigned char>& data) {
        std::vector<unsigned char> encrypted(data.size());
        CAST_KEY castKey;
        
        CAST_set_key(&castKey, 7, key56);
        
        for (size_t i = 0; i < data.size(); i += CAST_BLOCK) {
            size_t blockSize = std::min(static_cast<size_t>(CAST_BLOCK), 
                                       data.size() - i);
            unsigned char input[CAST_BLOCK] = {0};
            unsigned char output[CAST_BLOCK];
            
            memcpy(input, &data[i], blockSize);
            CAST_ecb_encrypt(input, output, &castKey, CAST_ENCRYPT);
            memcpy(&encrypted[i], output, blockSize);
        }
        
        return encrypted;
    }
    
    std::vector<unsigned char> encryptRC4(const std::vector<unsigned char>& data) {
        std::vector<unsigned char> encrypted(data.size());
        RC4_KEY rc4Key;
        
        RC4_set_key(&rc4Key, 7, key56);
        RC4(&rc4Key, data.size(), data.data(), encrypted.data());
        
        return encrypted;
    }
    
    std::vector<unsigned char> encryptDES(const std::vector<unsigned char>& data) {
        std::vector<unsigned char> encrypted(data.size());
        DES_key_schedule desKey;
        
        unsigned char desKey8[8];
        memcpy(desKey8, key56, 7);
        desKey8[7] = 0;
        
        DES_set_odd_parity(reinterpret_cast<DES_cblock*>(desKey8));
        DES_set_key_unchecked(reinterpret_cast<const_DES_cblock*>(desKey8), &desKey);
        
        for (size_t i = 0; i < data.size(); i += 8) {
            size_t blockSize = std::min(static_cast<size_t>(8), data.size() - i);
            unsigned char input[8] = {0};
            unsigned char output[8];
            
            memcpy(input, &data[i], blockSize);
            DES_ecb_encrypt(reinterpret_cast<const_DES_cblock*>(input),
                          reinterpret_cast<DES_cblock*>(output),
                          &desKey, DES_ENCRYPT);
            memcpy(&encrypted[i], output, blockSize);
        }
        
        return encrypted;
    }
    
    std::vector<unsigned char> encryptBlowfish(const std::vector<unsigned char>& data) {
        std::vector<unsigned char> encrypted(data.size());
        BF_KEY bfKey;
        
        BF_set_key(&bfKey, 7, key56);
        
        for (size_t i = 0; i < data.size(); i += BF_BLOCK) {
            size_t blockSize = std::min(static_cast<size_t>(BF_BLOCK), 
                                       data.size() - i);
            unsigned char input[BF_BLOCK] = {0};
            unsigned char output[BF_BLOCK];
            
            memcpy(input, &data[i], blockSize);
            BF_ecb_encrypt(input, output, &bfKey, BF_ENCRYPT);
            memcpy(&encrypted[i], output, blockSize);
        }
        
        return encrypted;
    }
    
    bool writeChunkToFile(std::ofstream& file, const std::vector<unsigned char>& data) {
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        return file.good();
    }
    
    void createDirectory(const std::string& dir) {
        std::filesystem::create_directories(dir);
    }
    
    std::string formatBytes(size_t bytes) {
        if (bytes < 1024) return std::to_string(bytes) + " B";
        if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
        if (bytes < 1024 * 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
        return std::to_string(bytes / (1024ULL * 1024ULL * 1024ULL)) + " GB";
    }

public:
    CiphertextGenerator(unsigned int baseSeed) : generator(baseSeed) {
        generate56BitKey(baseSeed);
    }
    
    void generateCiphertexts(const std::string& outputDir = "ciphertexts") {
        // Utwórz katalog główny
        createDirectory(outputDir);
        
        // Katalogi dla każdego algorytmu
        std::vector<std::string> algorithms = {"cast", "rc4", "des", "blowfish"};
        
        std::cout << "Generowanie szyfrogramów..." << std::endl;
        std::cout << "Rozmiar każdego pliku: " << formatBytes(FILE_SIZE_BYTES) << std::endl;
        std::cout << "Klucz 56-bit: ";
        for (int i = 0; i < 7; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(key56[i]);
        }
        std::cout << std::dec << std::endl << std::endl;
        
        size_t totalWritten = 0;
        
        for (const auto& alg : algorithms) {
            std::string algDir = outputDir + "/" + alg;
            createDirectory(algDir);
            
            std::string filepath = algDir + "/" + alg + ".bin";
            
            std::cout << "Generowanie pliku dla algorytmu: " << alg << std::endl;
            std::cout << "  Plik: " << filepath << std::endl;
            
            // Otwórz plik do zapisu
            std::ofstream file(filepath, std::ios::binary);
            if (!file.is_open()) {
                std::cerr << "  Błąd: Nie można otworzyć pliku " << filepath << std::endl;
                continue;
            }
            
            size_t bytesWritten = 0;
            unsigned int baseSeed = generator();
            unsigned int chunkSeed = baseSeed + (alg == "cast" ? 0 : alg == "rc4" ? 10000 : 
                                                  alg == "des" ? 20000 : 30000);
            size_t lastProgressReport = 0;
            
            // Generuj i zapisuj w chunkach
            while (bytesWritten < FILE_SIZE_BYTES) {
                size_t currentChunkSize = std::min(CHUNK_SIZE, FILE_SIZE_BYTES - bytesWritten);
                
                // Generuj dane losowe dla chunka
                std::vector<unsigned char> randomData;
                generateRandomData(randomData, currentChunkSize, chunkSeed + bytesWritten);
                
                // Szyfruj danymi algorytmem
                std::vector<unsigned char> encrypted;
                if (alg == "cast") {
                    encrypted = encryptCAST(randomData);
                } else if (alg == "rc4") {
                    encrypted = encryptRC4(randomData);
                } else if (alg == "des") {
                    encrypted = encryptDES(randomData);
                } else if (alg == "blowfish") {
                    encrypted = encryptBlowfish(randomData);
                }
                
                // Zapisz chunk do pliku
                if (!writeChunkToFile(file, encrypted)) {
                    std::cerr << "  Błąd przy zapisie do pliku " << filepath << std::endl;
                    file.close();
                    break;
                }
                
                bytesWritten += encrypted.size();
                
                // Wyświetl postęp co 500 MB lub na końcu
                size_t progressInterval = 500 * 1024 * 1024; // 500 MB
                if (bytesWritten - lastProgressReport >= progressInterval || 
                    bytesWritten >= FILE_SIZE_BYTES) {
                    double progress = (static_cast<double>(bytesWritten) / FILE_SIZE_BYTES) * 100.0;
                    std::cout << "  Postęp: " << std::fixed << std::setprecision(1) << progress 
                              << "% (" << formatBytes(bytesWritten) << " / " 
                              << formatBytes(FILE_SIZE_BYTES) << ")" << std::endl;
                    lastProgressReport = bytesWritten;
                }
            }
            
            file.close();
            
            if (bytesWritten == FILE_SIZE_BYTES) {
                totalWritten += bytesWritten;
                std::cout << "  ✓ Zapisano: " << filepath 
                          << " (" << formatBytes(bytesWritten) << ")" << std::endl;
            } else {
                std::cerr << "  ✗ Nie udało się zapisać pełnego pliku" << std::endl;
            }
            
            std::cout << std::endl;
        }
        
        std::cout << "Zakończono generowanie szyfrogramów." << std::endl;
        std::cout << "Łącznie zapisano: " << formatBytes(totalWritten) << std::endl;
        std::cout << "Pliki znajdują się w katalogu: " << outputDir << std::endl;
    }
};

int main(int argc, char* argv[]) {
    unsigned int seed = 12345;
    std::string outputDir = "ciphertexts";
    
    if (argc > 1) {
        seed = std::stoul(argv[1]);
    }
    if (argc > 2) {
        outputDir = argv[2];
    }
    
    std::cout << "=== Generator szyfrogramów (8 GB każdy) ===" << std::endl;
    std::cout << "Ziarno generatora: " << seed << std::endl;
    std::cout << "Katalog wyjściowy: " << outputDir << std::endl;
    std::cout << std::endl;
    
    CiphertextGenerator generator(seed);
    generator.generateCiphertexts(outputDir);
    
    return 0;
}

#pragma GCC diagnostic pop
