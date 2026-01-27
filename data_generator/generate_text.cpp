#include "text_generator.h"
#include <iostream>
#include <thread>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <cmath>

int main(int argc, char* argv[]) {
    unsigned int seed = 12345;
    std::string outputDir = "text_files";
    double fileSizeGB = 8.0;
    int numFiles = 1;
    
    // Parsowanie argumentów
    if (argc > 1) {
        seed = std::stoul(argv[1]);
    }
    if (argc > 2) {
        outputDir = argv[2];
    }
    if (argc > 3) {
        fileSizeGB = std::stod(argv[3]);
    }
    if (argc > 4) {
        numFiles = std::stoi(argv[4]);
    }
    
    std::cout << "=== Generator plików tekstowych (angielski) ===" << std::endl;
    std::cout << "Ziarno generatora: " << seed << std::endl;
    std::cout << "Katalog wyjściowy: " << outputDir << std::endl;
    std::cout << "Rozmiar każdego pliku: " << std::fixed << std::setprecision(3) << fileSizeGB << " GB" << std::endl;
    std::cout << "Liczba plików: " << numFiles << std::endl;
    std::cout << std::endl;
    
    // Utwórz katalog wyjściowy
    std::filesystem::create_directories(outputDir);
    
    // Generuj pliki
    size_t fileSizeBytes = static_cast<size_t>(fileSizeGB * 1024.0 * 1024.0 * 1024.0);
    
    if (numFiles == 1) {
        // Pojedynczy plik
        std::string outputPath = outputDir + "/english_text_" + std::to_string(seed) + ".txt";
        TextGenerator generator(seed);
        generator.generateTextFile(outputPath, fileSizeBytes);
    } else {
        // Wiele plików w osobnych wątkach
        std::vector<std::thread> threads;
        
        for (int i = 0; i < numFiles; i++) {
            unsigned int fileSeed = seed + i * 10000;
            std::string outputPath = outputDir + "/english_text_" + std::to_string(fileSeed) + ".txt";
            
            threads.emplace_back([fileSeed, outputPath, fileSizeBytes]() {
                TextGenerator generator(fileSeed);
                generator.generateTextFile(outputPath, fileSizeBytes);
            });
        }
        
        // Poczekaj na zakończenie wszystkich wątków
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << std::endl;
        std::cout << "Zakończono generowanie " << numFiles << " plików tekstowych." << std::endl;
        std::cout << "Pliki znajdują się w katalogu: " << outputDir << std::endl;
    }
    
    return 0;
}
