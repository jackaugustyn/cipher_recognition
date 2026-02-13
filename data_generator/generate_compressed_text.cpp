#include "text_generator.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <cstring>
#include <functional>
#include <zlib.h>
#include <fcntl.h>
#include <unistd.h>

/**
 * Moduł kompresujący dane tekstowe do pliku o rozmiarze 8 GB.
 * Czyta tekst z pliku lub generuje go, kompresuje strumieniowo (gzip)
 * i zapisuje do pliku .gz do momentu osiągnięcia ~8 GB skompresowanych danych.
 */

namespace {

const size_t FILE_SIZE_GB = 8;
const size_t FILE_SIZE_BYTES = FILE_SIZE_GB * 1024ULL * 1024ULL * 1024ULL;
const size_t CHUNK_INPUT = 4 * 1024 * 1024;   // 4 MB wejścia na iterację

std::string formatBytes(size_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    if (bytes < 1024 * 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
    return std::to_string(bytes / (1024ULL * 1024ULL * 1024ULL)) + " GB";
}

void createDirectory(const std::string& dir) {
    if (!dir.empty()) {
        std::filesystem::path p(dir);
        if (p.has_parent_path())
            std::filesystem::create_directories(p.parent_path());
    }
}

} // namespace

class TextCompressor {
public:
    TextCompressor() = default;

    /**
     * Kompresuje dane z pliku wejściowego do pliku .gz o rozmiarze do ~8 GB.
     */
    bool compressFileTo8GB(const std::string& inputPath, const std::string& outputPath) {
        std::ifstream in(inputPath, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "Błąd: Nie można otworzyć pliku wejściowego: " << inputPath << std::endl;
            return false;
        }
        createDirectory(outputPath);
        return compressStreamTo8GB(
            [&in](unsigned char* buf, size_t maxLen) -> size_t {
                in.read(reinterpret_cast<char*>(buf), maxLen);
                return static_cast<size_t>(in.gcount());
            },
            outputPath,
            "plik: " + inputPath
        );
    }

    /**
     * Generuje tekst, kompresuje go strumieniowo i zapisuje do pliku .gz ~8 GB.
     */
    bool generateAndCompressTo8GB(const std::string& outputPath, unsigned int seed) {
        std::cout << "=== Kompresja danych tekstowych do pliku 8 GB ===" << std::endl;
        std::cout << "Ziarno: " << seed << std::endl;
        std::cout << "Wyjście: " << outputPath << std::endl;
        std::cout << "Cel: ~" << FILE_SIZE_GB << " GB skompresowanych danych (gzip)" << std::endl << std::endl;

        createDirectory(outputPath);
        TextGenerator gen(seed);
        size_t totalGenerated = 0;
        unsigned int runSeed = seed;
        bool ok = compressStreamTo8GB(
            [this, &gen, &totalGenerated, &runSeed](unsigned char* buf, size_t maxLen) -> size_t {
                gen.generateTextToBuffer(bufferForGenerator, maxLen, runSeed);
                size_t n = bufferForGenerator.size();
                if (n > 0) {
                    memcpy(buf, bufferForGenerator.data(), n);
                    totalGenerated += n;
                }
                return n;
            },
            outputPath,
            "generator (ziarno " + std::to_string(seed) + ")"
        );
        if (ok)
            std::cout << "Wygenerowano i skompresowano łącznie " << formatBytes(totalGenerated) << " danych wejściowych." << std::endl;
        return ok;
    }

private:
    std::vector<unsigned char> bufferForGenerator;

    using ReadChunkFn = std::function<size_t(unsigned char* buf, size_t maxLen)>;

    bool compressStreamTo8GB(ReadChunkFn readChunk, const std::string& outputPath, const std::string& sourceDesc) {
        (void)sourceDesc;
        int fd = open(outputPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            std::cerr << "Błąd: Nie można utworzyć pliku wyjściowego: " << outputPath << std::endl;
            return false;
        }

        gzFile gz = gzdopen(fd, "wb");
        if (!gz) {
            close(fd);
            std::cerr << "Błąd: gzdopen" << std::endl;
            return false;
        }

        gzsetparams(gz, 6, Z_DEFAULT_STRATEGY);

        std::vector<unsigned char> inBuf(CHUNK_INPUT);
        const size_t progressInterval = 500 * 1024 * 1024; // 500 MB
        size_t lastReport = 0;

        while (true) {
            off_t currentSize = lseek(fd, 0, SEEK_END);
            if (currentSize < 0) {
                std::cerr << "Błąd lseek" << std::endl;
                gzclose(gz);
                return false;
            }
            if (static_cast<size_t>(currentSize) >= FILE_SIZE_BYTES)
                break;

            size_t toRead = CHUNK_INPUT;
            size_t n = readChunk(inBuf.data(), toRead);
            if (n == 0)
                break;

            int written = gzwrite(gz, inBuf.data(), static_cast<unsigned>(n));
            if (written <= 0 || static_cast<size_t>(written) != n) {
                std::cerr << "Błąd zapisu gzwrite przy " << formatBytes(static_cast<size_t>(currentSize)) << std::endl;
                gzclose(gz);
                return false;
            }

            if (gzflush(gz, Z_SYNC_FLUSH) != Z_OK) {
                std::cerr << "Błąd gzflush" << std::endl;
                gzclose(gz);
                return false;
            }

            currentSize = lseek(fd, 0, SEEK_END);
            size_t totalWritten = static_cast<size_t>(currentSize);

            if (totalWritten - lastReport >= progressInterval || totalWritten >= FILE_SIZE_BYTES) {
                double pct = 100.0 * static_cast<double>(totalWritten) / FILE_SIZE_BYTES;
                std::cout << "  Postęp: " << std::fixed << std::setprecision(1) << pct << "% ("
                          << formatBytes(totalWritten) << " / " << formatBytes(FILE_SIZE_BYTES) << ")" << std::endl;
                lastReport = totalWritten;
            }
        }

        if (gzclose(gz) != Z_OK) {
            std::cerr << "Błąd przy zamykaniu pliku gzip." << std::endl;
            return false;
        }
        size_t finalSize = static_cast<size_t>(std::filesystem::file_size(outputPath));
        std::cout << "  Zapisano: " << outputPath << " (" << formatBytes(finalSize) << ")" << std::endl;
        return true;
    }
};

int main(int argc, char* argv[]) {
    unsigned int seed = 12345;
    std::string outputDir = "compressed_text";
    std::string outputFile;
    std::string inputFile;

    if (argc > 1) {
        std::string a1 = argv[1];
        if (a1.find('.') != std::string::npos && (a1.find(".txt") != std::string::npos || a1.find(".bin") != std::string::npos))
            inputFile = a1;
        else
            seed = static_cast<unsigned int>(std::stoul(a1));
    }
    if (argc > 2) {
        if (inputFile.empty())
            outputDir = argv[2];
        else
            seed = static_cast<unsigned int>(std::stoul(argv[2]));
    }
    if (argc > 3)
        outputDir = argv[3];

    if (outputFile.empty())
        outputFile = outputDir + "/compressed_" + std::to_string(seed) + ".gz";

    TextCompressor compressor;

    if (!inputFile.empty()) {
        if (!compressor.compressFileTo8GB(inputFile, outputFile))
            return 1;
    } else {
        if (!compressor.generateAndCompressTo8GB(outputFile, seed))
            return 1;
    }

    std::cout << "Zakończono." << std::endl;
    return 0;
}
