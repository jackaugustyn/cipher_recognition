#ifndef TEXT_GENERATOR_H
#define TEXT_GENERATOR_H

#include <string>
#include <vector>
#include <random>
#include <fstream>
#include <mutex>
#include <map>
#include <unordered_map>

class TextGenerator {
private:
    std::mt19937 generator;
    std::mutex coutMutex;
    
    // Markov chain structures
    std::unordered_map<std::string, std::vector<std::string>> markovChain;
    std::vector<std::string> sentenceStarters;
    
    void initializeMarkovChain();
    void addToMarkovChain(const std::string& text);
    std::vector<std::string> tokenize(const std::string& text);
    std::string generateSentenceMarkov(unsigned int& seed);
    std::string generateParagraph(unsigned int& seed, size_t targetSize);
    std::string formatBytes(size_t bytes);
    void createDirectory(const std::string& dir);

public:
    TextGenerator(unsigned int seed);
    void generateTextFile(const std::string& outputPath, size_t targetSizeBytes);
    void generateTextToBuffer(std::vector<unsigned char>& buffer, size_t targetSizeBytes, unsigned int& seed);
};

#endif // TEXT_GENERATOR_H
