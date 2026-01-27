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

class TextEncryptor {
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
    
    void encryptTextFile(const std::string& inputPath, const std::string& outputPath, 
                       const std::string& algorithm) {
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "Szyfrowanie pliku algorytmem: " << algorithm << std::endl;
            std::cout << "  Wejście: " << inputPath << std::endl;
            std::cout << "  Wyjście: " << outputPath << std::endl;
        }
        
        // Utwórz katalog wyjściowy
        std::filesystem::path path(outputPath);
        if (path.has_parent_path()) {
            createDirectory(path.parent_path().string());
        }
        
        std::ifstream inputFile(inputPath, std::ios::binary);
        if (!inputFile.is_open()) {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cerr << "  Błąd: Nie można otworzyć pliku wejściowego " << inputPath << std::endl;
            return;
        }
        
        std::ofstream outputFile(outputPath, std::ios::binary);
        if (!outputFile.is_open()) {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cerr << "  Błąd: Nie można otworzyć pliku wyjściowego " << outputPath << std::endl;
            inputFile.close();
            return;
        }
        
        size_t bytesProcessed = 0;
        size_t lastProgressReport = 0;
        const size_t progressInterval = 500 * 1024 * 1024; // 500 MB
        
        while (inputFile.good() && bytesProcessed < FILE_SIZE_BYTES) {
            size_t remaining = FILE_SIZE_BYTES - bytesProcessed;
            size_t currentChunkSize = std::min(CHUNK_SIZE, remaining);
            
            // Przeczytaj chunk z pliku tekstowego
            std::vector<unsigned char> textChunk(currentChunkSize);
            inputFile.read(reinterpret_cast<char*>(textChunk.data()), currentChunkSize);
            size_t bytesRead = inputFile.gcount();
            
            if (bytesRead == 0) break;
            
            // Dostosuj rozmiar jeśli przeczytano mniej
            if (bytesRead < currentChunkSize) {
                textChunk.resize(bytesRead);
            }
            
            // Szyfruj chunk
            std::vector<unsigned char> encrypted;
            if (algorithm == "cast") {
                encrypted = encryptCAST(textChunk);
            } else if (algorithm == "rc4") {
                encrypted = encryptRC4(textChunk);
            } else if (algorithm == "des") {
                encrypted = encryptDES(textChunk);
            } else if (algorithm == "blowfish") {
                encrypted = encryptBlowfish(textChunk);
            }
            
            // Zapisz zaszyfrowany chunk
            if (!writeChunkToFile(outputFile, encrypted)) {
                std::lock_guard<std::mutex> lock(coutMutex);
                std::cerr << "  Błąd przy zapisie do pliku " << outputPath << std::endl;
                break;
            }
            
            bytesProcessed += bytesRead;
            
            // Wyświetl postęp
            if (bytesProcessed - lastProgressReport >= progressInterval ||
                bytesProcessed >= FILE_SIZE_BYTES) {
                std::lock_guard<std::mutex> lock(coutMutex);
                double progress = (static_cast<double>(bytesProcessed) / FILE_SIZE_BYTES) * 100.0;
                std::cout << "  [" << algorithm << "] Postęp: " << std::fixed << std::setprecision(1) << progress
                          << "% (" << formatBytes(bytesProcessed) << " / "
                          << formatBytes(FILE_SIZE_BYTES) << ")" << std::endl;
                lastProgressReport = bytesProcessed;
            }
        }
        
        inputFile.close();
        outputFile.close();
        
        std::lock_guard<std::mutex> lock(coutMutex);
        if (bytesProcessed > 0) {
            std::cout << "  ✓ [" << algorithm << "] Zapisano: " << outputPath
                      << " (" << formatBytes(bytesProcessed) << ")" << std::endl;
        } else {
            std::cerr << "  ✗ [" << algorithm << "] Nie udało się zaszyfrować pliku" << std::endl;
        }
    }

public:
    TextEncryptor(unsigned int seed) {
        generate56BitKey(seed);
    }
    
    void encryptExistingFile(const std::string& inputPath, const std::string& outputDir = "encrypted_text", unsigned int seed = 12345) {
        // Utwórz katalog główny
        createDirectory(outputDir);
        
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "=== Szyfrowanie istniejącego pliku ===" << std::endl;
            std::cout << "Plik wejściowy: " << inputPath << std::endl;
            std::cout << "Ziarno generatora: " << seed << std::endl;
            std::cout << "Katalog wyjściowy: " << outputDir << std::endl;
            std::cout << "Klucz 56-bit: ";
            for (int i = 0; i < 7; i++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(key56[i]);
            }
            std::cout << std::dec << std::endl << std::endl;
        }
        
        // Algorytmy do przetworzenia
        std::vector<std::string> algorithms = {"cast", "rc4", "des", "blowfish"};
        
        // Utwórz katalogi dla każdego algorytmu
        for (const auto& alg : algorithms) {
            std::string algDir = outputDir + "/" + alg;
            createDirectory(algDir);
        }
        
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "Szyfrowanie tekstu wszystkimi algorytmami..." << std::endl;
            std::cout << std::endl;
        }
        
        // Szyfruj każdy algorytm w osobnym wątku
        std::vector<std::thread> threads;
        for (const auto& alg : algorithms) {
            std::string outputPath = outputDir + "/" + alg + "/encrypted_" + alg + "_" + std::to_string(seed) + ".bin";
            threads.emplace_back(&TextEncryptor::encryptTextFile,
                                this, inputPath, outputPath, alg);
        }
        
        // Poczekaj na zakończenie wszystkich wątków
        for (auto& thread : threads) {
            thread.join();
        }
        
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << std::endl;
            std::cout << "Zakończono szyfrowanie." << std::endl;
            std::cout << "Plik wejściowy: " << inputPath << std::endl;
            std::cout << "Zaszyfrowane pliki znajdują się w katalogu: " << outputDir << std::endl;
        }
    }
    
    void generateAndEncrypt(const std::string& outputDir = "encrypted_text", unsigned int seed = 12345) {
        // Utwórz katalog główny
        createDirectory(outputDir);
        
        // Najpierw wygeneruj plik tekstowy
        std::string textFilePath = outputDir + "/plaintext_" + std::to_string(seed) + ".txt";
        
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "=== Generator i szyfrowanie tekstu (8 GB) ===" << std::endl;
            std::cout << "Ziarno generatora: " << seed << std::endl;
            std::cout << "Katalog wyjściowy: " << outputDir << std::endl;
            std::cout << "Klucz 56-bit: ";
            for (int i = 0; i < 7; i++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(key56[i]);
            }
            std::cout << std::dec << std::endl << std::endl;
            std::cout << "Krok 1: Generowanie pliku tekstowego (8 GB)..." << std::endl;
        }
        
        TextGenerator textGen(seed);
        textGen.generateTextFile(textFilePath, FILE_SIZE_BYTES);
        
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << std::endl;
            std::cout << "Krok 2: Szyfrowanie tekstu wszystkimi algorytmami..." << std::endl;
            std::cout << std::endl;
        }
        
        // Algorytmy do przetworzenia
        std::vector<std::string> algorithms = {"cast", "rc4", "des", "blowfish"};
        
        // Utwórz katalogi dla każdego algorytmu
        for (const auto& alg : algorithms) {
            std::string algDir = outputDir + "/" + alg;
            createDirectory(algDir);
        }
        
        // Szyfruj każdy algorytm w osobnym wątku
        std::vector<std::thread> threads;
        for (const auto& alg : algorithms) {
            std::string outputPath = outputDir + "/" + alg + "/encrypted_" + alg + "_" + std::to_string(seed) + ".bin";
            threads.emplace_back(&TextEncryptor::encryptTextFile,
                                this, textFilePath, outputPath, alg);
        }
        
        // Poczekaj na zakończenie wszystkich wątków
        for (auto& thread : threads) {
            thread.join();
        }
        
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << std::endl;
            std::cout << "Zakończono generowanie i szyfrowanie." << std::endl;
            std::cout << "Plik tekstowy: " << textFilePath << std::endl;
            std::cout << "Zaszyfrowane pliki znajdują się w katalogu: " << outputDir << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    unsigned int seed = 12345;
    std::string outputDir = "encrypted_text";
    std::string inputFile = "";
    
    if (argc > 1) {
        // Jeśli pierwszy argument to plik (zawiera .txt lub .bin), użyj go jako wejścia
        std::string arg1 = argv[1];
        if (arg1.find('.') != std::string::npos) {
            inputFile = arg1;
        } else {
            seed = std::stoul(argv[1]);
        }
    }
    if (argc > 2) {
        if (inputFile.empty()) {
            outputDir = argv[2];
        } else {
            seed = std::stoul(argv[2]);
        }
    }
    if (argc > 3) {
        outputDir = argv[3];
    }
    
    TextEncryptor encryptor(seed);
    
    if (!inputFile.empty()) {
        // Szyfruj istniejący plik
        encryptor.encryptExistingFile(inputFile, outputDir, seed);
    } else {
        // Generuj i szyfruj
        encryptor.generateAndEncrypt(outputDir, seed);
    }
    
    return 0;
}

#pragma GCC diagnostic pop
