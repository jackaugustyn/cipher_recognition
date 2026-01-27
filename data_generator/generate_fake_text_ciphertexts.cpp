#include "text_generator.h"
#include <iostream>
#include <vector>
#include <random>
#include <iomanip>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <thread>
#include <mutex>
#include <openssl/des.h>
#include <openssl/blowfish.h>
#include <openssl/cast.h>
#include <openssl/rc4.h>

// Wyciszenie ostrzeżeń o przestarzałych funkcjach OpenSSL
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

class FakeTextCiphertextGenerator {
private:
    unsigned char key56[7]; // 56 bits = 7 bytes
    std::mutex coutMutex;
    const size_t FILE_SIZE_GB = 8;
    const size_t FILE_SIZE_BYTES = FILE_SIZE_GB * 1024ULL * 1024ULL * 1024ULL;
    const size_t CHUNK_SIZE = 100 * 1024 * 1024; // 100 MB chunków
    
    void generate56BitKey(unsigned int seed) {
        std::mt19937 keyGen(seed);
        std::uniform_int_distribution<unsigned char> dist(0, 255);
        for (int i = 0; i < 7; i++) {
            key56[i] = dist(keyGen);
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
    
    // Generuje tekst angielski w chunkach
    void generateTextChunk(std::vector<unsigned char>& textData, size_t targetSize, unsigned int& seed) {
        // Użyj TextGenerator do generowania tekstu
        TextGenerator textGen(seed);
        textGen.generateTextToBuffer(textData, targetSize, seed);
    }

public:
    FakeTextCiphertextGenerator(unsigned int baseSeed) {
        generate56BitKey(baseSeed);
    }

    void generateCiphertextForAlgorithm(const std::string& alg, const std::string& outputDir, unsigned int baseSeed) {
        std::string algDir = outputDir + "/" + alg;
        createDirectory(algDir);

        std::ostringstream filename;
        filename << algDir << "/" << alg << "_from_text_" << baseSeed << ".bin";
        std::string filepath = filename.str();

        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "Generowanie szyfrogramu dla algorytmu: " << alg << std::endl;
            std::cout << "  Plik: " << filepath << std::endl;
        }

        // Otwórz plik do zapisu
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cerr << "  Błąd: Nie można otworzyć pliku " << filepath << std::endl;
            return;
        }

        size_t bytesWritten = 0;
        unsigned int chunkSeed = baseSeed + (alg == "cast" ? 0 : alg == "rc4" ? 10000 :
                                              alg == "des" ? 20000 : 30000);
        size_t lastProgressReport = 0;

        // Generuj tekst i szyfruj w chunkach
        while (bytesWritten < FILE_SIZE_BYTES) {
            size_t currentChunkSize = std::min(CHUNK_SIZE, FILE_SIZE_BYTES - bytesWritten);

            // Generuj tekst angielski dla chunka
            std::vector<unsigned char> textChunk;
            unsigned int localSeed = chunkSeed + (bytesWritten / CHUNK_SIZE);
            generateTextChunk(textChunk, currentChunkSize, localSeed);
            chunkSeed = localSeed; // Zaktualizuj seed dla następnego chunka

            // Szyfruj tekst algorytmem
            std::vector<unsigned char> encrypted;
            if (alg == "cast") {
                encrypted = encryptCAST(textChunk);
            } else if (alg == "rc4") {
                encrypted = encryptRC4(textChunk);
            } else if (alg == "des") {
                encrypted = encryptDES(textChunk);
            } else if (alg == "blowfish") {
                encrypted = encryptBlowfish(textChunk);
            }

            // Zapisz chunk do pliku
            if (!writeChunkToFile(file, encrypted)) {
                std::lock_guard<std::mutex> lock(coutMutex);
                std::cerr << "  Błąd przy zapisie do pliku " << filepath << std::endl;
                file.close();
                return;
            }

            bytesWritten += encrypted.size();

            // Wyświetl postęp co 500 MB lub na końcu
            size_t progressInterval = 500 * 1024 * 1024; // 500 MB
            if (bytesWritten - lastProgressReport >= progressInterval ||
                bytesWritten >= FILE_SIZE_BYTES) {
                std::lock_guard<std::mutex> lock(coutMutex);
                double progress = (static_cast<double>(bytesWritten) / FILE_SIZE_BYTES) * 100.0;
                std::cout << "  [" << alg << "] Postęp: " << std::fixed << std::setprecision(1) << progress
                          << "% (" << formatBytes(bytesWritten) << " / "
                          << formatBytes(FILE_SIZE_BYTES) << ")" << std::endl;
                lastProgressReport = bytesWritten;
            }
        }

        file.close();

        std::lock_guard<std::mutex> lock(coutMutex);
        if (bytesWritten == FILE_SIZE_BYTES) {
            std::cout << "  ✓ [" << alg << "] Zapisano: " << filepath
                      << " (" << formatBytes(bytesWritten) << ")" << std::endl;
        } else {
            std::cerr << "  ✗ [" << alg << "] Nie udało się zapisać pełnego pliku" << std::endl;
        }
    }
    
    void generateCiphertexts(const std::string& outputDir = "fake_text_ciphertexts", unsigned int seed = 12345) {
        // Utwórz katalog główny
        createDirectory(outputDir);

        // Algorytmy do przetworzenia
        std::vector<std::string> algorithms = {"blowfish", "cast", "des", "rc4"};

        std::cout << "Generowanie szyfrogramów z tekstu angielskiego..." << std::endl;
        std::cout << "Rozmiar każdego pliku: " << formatBytes(FILE_SIZE_BYTES) << std::endl;
        std::cout << "Klucz 56-bit: ";
        for (int i = 0; i < 7; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(key56[i]);
        }
        std::cout << std::dec << std::endl << std::endl;

        // Uruchom każdy algorytm w osobnym wątku
        std::vector<std::thread> threads;
        for (const auto& alg : algorithms) {
            threads.emplace_back(&FakeTextCiphertextGenerator::generateCiphertextForAlgorithm,
                                this, alg, outputDir, seed);
        }

        // Poczekaj na zakończenie wszystkich wątków
        for (auto& thread : threads) {
            thread.join();
        }

        size_t totalWritten = algorithms.size() * FILE_SIZE_BYTES;
        std::cout << std::endl;
        std::cout << "Zakończono generowanie szyfrogramów." << std::endl;
        std::cout << "Łącznie zapisano: " << formatBytes(totalWritten) << std::endl;
        std::cout << "Pliki znajdują się w katalogu: " << outputDir << std::endl;
    }
};

int main(int argc, char* argv[]) {
    unsigned int seed = 12345;
    std::string outputDir = "fake_text_ciphertexts";
    
    if (argc > 1) {
        seed = std::stoul(argv[1]);
    }
    if (argc > 2) {
        outputDir = argv[2];
    }
    
    std::cout << "=== Generator szyfrogramów z tekstu angielskiego (8 GB każdy) ===" << std::endl;
    std::cout << "Ziarno generatora: " << seed << std::endl;
    std::cout << "Katalog wyjściowy: " << outputDir << std::endl;
    std::cout << std::endl;
    
    FakeTextCiphertextGenerator generator(seed);
    generator.generateCiphertexts(outputDir, seed);
    
    return 0;
}

#pragma GCC diagnostic pop
