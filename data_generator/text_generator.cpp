#include "text_generator.h"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <cctype>

std::vector<std::string> TextGenerator::tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : text) {
        if (std::isspace(c) || std::ispunct(c)) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            if (std::ispunct(c) && c != '\'' && c != '-') {
                tokens.push_back(std::string(1, c));
            }
        } else {
            current += std::tolower(c);
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

void TextGenerator::addToMarkovChain(const std::string& text) {
    std::vector<std::string> tokens = tokenize(text);
    if (tokens.size() < 2) return;
    
    // Zapisz pierwsze słowo jako starter
    if (!tokens.empty() && tokens[0] != "." && tokens[0] != "!" && tokens[0] != "?") {
        sentenceStarters.push_back(tokens[0]);
    }
    
    // Buduj łańcuch Markowa (bigramy - każde słowo zależy od poprzedniego)
    for (size_t i = 0; i < tokens.size() - 1; i++) {
        std::string current = tokens[i];
        std::string next = tokens[i + 1];
        
        // Pomijaj interpunkcję jako klucze
        if (current == "." || current == "!" || current == "?" || 
            current == "," || current == ";" || current == ":") {
            continue;
        }
        
        markovChain[current].push_back(next);
    }
}

void TextGenerator::initializeMarkovChain() {
    // Korpus realistycznych zdań angielskich do treningu łańcucha Markowa
    std::vector<std::string> corpus = {
        "The quick brown fox jumps over the lazy dog.",
        "In the beginning was the word and the word was with God.",
        "To be or not to be that is the question.",
        "It was the best of times it was the worst of times.",
        "All happy families are alike each unhappy family is unhappy in its own way.",
        "Call me Ishmael some years ago never mind how long precisely.",
        "It is a truth universally acknowledged that a single man in possession of a good fortune must be in want of a wife.",
        "The sun was shining on the sea shining with all his might.",
        "Once upon a time in a galaxy far far away.",
        "The old man and the sea was his favorite book.",
        "She walked down the street with confidence and purpose.",
        "The computer science department offers many interesting courses.",
        "Artificial intelligence is transforming the way we work and live.",
        "The weather today is beautiful with clear blue skies.",
        "They decided to go for a walk in the park.",
        "The meeting was scheduled for three o clock in the afternoon.",
        "She opened the door and stepped into the room.",
        "The book on the table belongs to my friend.",
        "We need to finish this project by the end of the week.",
        "The students were studying hard for their final exams.",
        "He picked up the phone and dialed the number.",
        "The restaurant serves delicious food at reasonable prices.",
        "They traveled across the country to visit their relatives.",
        "The company announced a new product launch next month.",
        "She wrote a letter to her grandmother last week.",
        "The movie was entertaining but the ending was disappointing.",
        "He enjoys reading books about history and science.",
        "The team worked together to solve the complex problem.",
        "They went shopping at the mall on Saturday afternoon.",
        "The teacher explained the lesson clearly to the students.",
        "She loves to play the piano in her spare time.",
        "The garden was full of beautiful flowers and plants.",
        "He decided to take a break from work and relax.",
        "The news about the accident spread quickly through the town.",
        "They built a new house on the hill overlooking the valley.",
        "The conference will be held in the convention center downtown.",
        "She received a scholarship to study at the university.",
        "The dog ran across the yard chasing the ball.",
        "He found the solution to the problem after hours of thinking.",
        "The library has an extensive collection of books and journals.",
        "They celebrated their anniversary with a romantic dinner.",
        "The artist painted a beautiful landscape of the countryside.",
        "She learned to speak French during her stay in Paris.",
        "The doctor recommended rest and plenty of fluids.",
        "He bought a new car with all the latest features.",
        "The children were playing in the park on a sunny day.",
        "They organized a charity event to help the homeless.",
        "The museum displays artifacts from ancient civilizations.",
        "She completed her degree in computer science with honors.",
        "The storm caused significant damage to the coastal areas.",
        "He enjoys cooking and trying new recipes from different countries.",
        "The company invested millions in research and development.",
        "They went on a vacation to the tropical island.",
        "The professor gave an interesting lecture on quantum physics.",
        "She started her own business selling handmade jewelry.",
        "The government announced new policies to improve education.",
        "He spent the weekend working on his home improvement project.",
        "The concert was sold out weeks before the event.",
        "They discussed the proposal during the board meeting.",
        "The novel tells the story of a young woman's journey.",
        "She volunteered at the local animal shelter on weekends.",
        "The technology has revolutionized the way we communicate.",
        "He received recognition for his outstanding contribution to science.",
        "The team won the championship after a thrilling final match.",
        "They explored the ancient ruins of the lost civilization.",
        "The restaurant offers a wide variety of international cuisine.",
        "She published her first novel to critical acclaim.",
        "The university offers scholarships to deserving students.",
        "He enjoys hiking in the mountains during summer months.",
        "The project requires collaboration between multiple departments.",
        "They organized a surprise party for their friend's birthday.",
        "The book provides valuable insights into human psychology.",
        "She learned to play the guitar by watching online tutorials.",
        "The company expanded its operations to new markets.",
        "He wrote a comprehensive report on climate change.",
        "The festival attracts thousands of visitors from around the world.",
        "They renovated their house to make it more energy efficient.",
        "The research team made a groundbreaking discovery in medicine.",
        "She started a blog to share her travel experiences.",
        "The school implemented new programs to support student learning.",
        "He enjoys photography and capturing moments of everyday life.",
        "The organization provides assistance to families in need.",
        "They celebrated the holiday with traditional food and music.",
        "The movie received several awards at the film festival.",
        "She completed a marathon training program and ran her first race.",
        "The company developed innovative solutions to environmental problems.",
        "He enjoys reading science fiction novels in his free time.",
        "The museum offers guided tours in multiple languages.",
        "They planted a garden with vegetables and herbs.",
        "The conference featured presentations by leading experts in the field.",
        "She started learning a new language to expand her horizons.",
        "The technology company announced plans for expansion into Asia.",
        "He enjoys woodworking and creating furniture in his workshop.",
        "The charity organization helps provide education to underprivileged children.",
        "They went on a road trip across the country.",
        "The book became a bestseller within weeks of publication.",
        "She received a promotion at work for her excellent performance.",
        "The university established a new research center for artificial intelligence.",
        "He enjoys playing chess and participating in tournaments.",
        "The restaurant chain opened new locations in several cities.",
        "They organized a community cleanup event in the neighborhood.",
        "The scientist published findings that could change our understanding of the universe.",
        "She started a fitness routine and noticed significant improvements.",
        "The company introduced flexible working hours for employees.",
        "He enjoys bird watching and documenting different species.",
        "The festival featured performances by local and international artists.",
        "They invested in renewable energy solutions for their home.",
        "The book explores themes of love loss and redemption.",
        "She completed an online course to improve her skills.",
        "The organization received funding to expand its programs.",
        "He enjoys gardening and growing his own vegetables.",
        "The conference addressed important issues facing the industry today."
    };
    
    // Dodaj wszystkie zdania z korpusu do łańcucha Markowa
    for (const auto& sentence : corpus) {
        addToMarkovChain(sentence);
    }
}

TextGenerator::TextGenerator(unsigned int seed) : generator(seed) {
    initializeMarkovChain();
}

std::string TextGenerator::generateSentenceMarkov(unsigned int& seed) {
    std::mt19937 localGen(seed++);
    
    if (markovChain.empty() || sentenceStarters.empty()) {
        return "The quick brown fox jumps over the lazy dog.";
    }
    
    std::string sentence;
    std::string currentWord;
    
    // Wybierz losowe słowo startowe
    std::uniform_int_distribution<size_t> starterDist(0, sentenceStarters.size() - 1);
    currentWord = sentenceStarters[starterDist(localGen)];
    // Kapitalizuj pierwsze słowo
    if (!currentWord.empty()) {
        currentWord[0] = std::toupper(currentWord[0]);
    }
    sentence = currentWord;
    
    // Generuj zdanie używając łańcucha Markowa
    int maxLength = 25;
    int wordCount = 1;
    std::string lowerCurrent = currentWord;
    std::transform(lowerCurrent.begin(), lowerCurrent.end(), lowerCurrent.begin(), ::tolower);
    
    while (wordCount < maxLength) {
        // Znajdź następne słowo w łańcuchu Markowa
        auto it = markovChain.find(lowerCurrent);
        if (it == markovChain.end() || it->second.empty()) {
            // Jeśli nie ma następnego słowa, zakończ zdanie
            break;
        }
        
        // Wybierz losowe następne słowo z możliwych
        std::uniform_int_distribution<size_t> nextDist(0, it->second.size() - 1);
        std::string nextWord = it->second[nextDist(localGen)];
        
        // Sprawdź czy to znak interpunkcyjny kończący zdanie
        if (nextWord == "." || nextWord == "!" || nextWord == "?") {
            sentence += nextWord;
            break;
        }
        
        // Dodaj przecinek jeśli potrzeba
        if (nextWord == ",") {
            sentence += nextWord;
            // Pobierz następne słowo po przecinku (pomijając przecinki)
            std::string commaNext;
            int attempts = 0;
            while (attempts < 10) {
                std::uniform_int_distribution<size_t> commaNextDist(0, it->second.size() - 1);
                commaNext = it->second[commaNextDist(localGen)];
                if (commaNext != "," && commaNext != "." && commaNext != "!" && commaNext != "?") {
                    sentence += " " + commaNext;
                    lowerCurrent = commaNext;
                    wordCount++;
                    break;
                }
                attempts++;
            }
            if (attempts >= 10) break;
            continue;
        }
        
        sentence += " " + nextWord;
        lowerCurrent = nextWord;
        wordCount++;
        
        // Losowo zakończ zdanie po minimum 8 słowach
        if (wordCount >= 8) {
            std::uniform_int_distribution<int> endSentence(0, 15);
            if (endSentence(localGen) < 2) {
                sentence += ".";
                break;
            }
        }
    }
    
    // Upewnij się, że zdanie kończy się interpunkcją
    if (sentence.back() != '.' && sentence.back() != '!' && sentence.back() != '?') {
        std::uniform_int_distribution<int> punctDist(0, 9);
        int punct = punctDist(localGen);
        if (punct < 8) {
            sentence += ".";
        } else if (punct == 8) {
            sentence += "!";
        } else {
            sentence += "?";
        }
    }
    
    return sentence;
}

std::string TextGenerator::generateParagraph(unsigned int& seed, size_t targetSize) {
    std::string paragraph;
    size_t currentSize = 0;
    unsigned int localSeed = seed;
    
    while (currentSize < targetSize) {
        std::string sentence = generateSentenceMarkov(localSeed);
        if (currentSize > 0) {
            paragraph += " ";
        }
        paragraph += sentence;
        currentSize = paragraph.size();
        
        // Jeśli przekroczymy cel, przerwij
        if (currentSize >= targetSize) {
            break;
        }
    }
    
    seed = localSeed;
    return paragraph;
}

std::string TextGenerator::formatBytes(size_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    if (bytes < 1024 * 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
    return std::to_string(bytes / (1024ULL * 1024ULL * 1024ULL)) + " GB";
}

void TextGenerator::createDirectory(const std::string& dir) {
    std::filesystem::create_directories(dir);
}

void TextGenerator::generateTextFile(const std::string& outputPath, size_t targetSizeBytes) {
    const size_t CHUNK_SIZE = 10 * 1024 * 1024; // 10 MB chunków tekstu
    const size_t PARAGRAPH_SIZE = 1000; // ~1000 znaków na akapit
    
    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "Generowanie pliku tekstowego..." << std::endl;
        std::cout << "  Plik: " << outputPath << std::endl;
        std::cout << "  Docelowy rozmiar: " << formatBytes(targetSizeBytes) << std::endl;
    }

    // Utwórz katalog jeśli potrzeba
    std::filesystem::path path(outputPath);
    if (path.has_parent_path()) {
        createDirectory(path.parent_path().string());
    }

    std::ofstream file(outputPath);
    if (!file.is_open()) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cerr << "  Błąd: Nie można otworzyć pliku " << outputPath << std::endl;
        return;
    }

    size_t bytesWritten = 0;
    unsigned int seed = generator();
    size_t lastProgressReport = 0;
    const size_t progressInterval = 500 * 1024 * 1024; // 500 MB

    while (bytesWritten < targetSizeBytes) {
        size_t remaining = targetSizeBytes - bytesWritten;
        size_t currentChunkSize = std::min(CHUNK_SIZE, remaining);
        
        // Generuj akapity do osiągnięcia rozmiaru chunka
        std::string chunk;
        while (chunk.size() < currentChunkSize && bytesWritten + chunk.size() < targetSizeBytes) {
            std::string paragraph = generateParagraph(seed, PARAGRAPH_SIZE);
            if (!chunk.empty()) {
                chunk += "\n\n"; // Dodaj podwójny enter między akapitami
            }
            chunk += paragraph;
        }
        
        // Obetnij do dokładnego rozmiaru jeśli przekroczono
        if (chunk.size() > remaining) {
            chunk = chunk.substr(0, remaining);
        }
        
        file << chunk;
        bytesWritten += chunk.size();
        
        // Wyświetl postęp
        if (bytesWritten - lastProgressReport >= progressInterval ||
            bytesWritten >= targetSizeBytes) {
            std::lock_guard<std::mutex> lock(coutMutex);
            double progress = (static_cast<double>(bytesWritten) / targetSizeBytes) * 100.0;
            std::cout << "  Postęp: " << std::fixed << std::setprecision(1) << progress
                      << "% (" << formatBytes(bytesWritten) << " / "
                      << formatBytes(targetSizeBytes) << ")" << std::endl;
            lastProgressReport = bytesWritten;
        }
    }

    file.close();

    std::lock_guard<std::mutex> lock(coutMutex);
    if (bytesWritten >= targetSizeBytes) {
        std::cout << "  ✓ Zapisano: " << outputPath
                  << " (" << formatBytes(bytesWritten) << ")" << std::endl;
    } else {
        std::cerr << "  ✗ Nie udało się zapisać pełnego pliku" << std::endl;
    }
}

void TextGenerator::generateTextToBuffer(std::vector<unsigned char>& buffer, size_t targetSizeBytes, unsigned int& seed) {
    const size_t PARAGRAPH_SIZE = 1000; // ~1000 znaków na akapit
    
    buffer.clear();
    buffer.reserve(targetSizeBytes);
    
    size_t bytesGenerated = 0;
    unsigned int localSeed = seed;
    
    while (bytesGenerated < targetSizeBytes) {
        std::string paragraph = generateParagraph(localSeed, PARAGRAPH_SIZE);
        
        // Dodaj paragraf do bufora
        size_t paragraphSize = paragraph.size();
        size_t remaining = targetSizeBytes - bytesGenerated;
        size_t bytesToAdd = std::min(paragraphSize, remaining);
        
        // Dodaj tekst do bufora
        buffer.insert(buffer.end(), paragraph.begin(), paragraph.begin() + bytesToAdd);
        bytesGenerated += bytesToAdd;
        
        // Jeśli nie dodaliśmy całego paragrafu, przerwij
        if (bytesToAdd < paragraphSize) {
            break;
        }
        
        // Dodaj podwójny enter między akapitami (jeśli jest miejsce)
        if (bytesGenerated + 2 <= targetSizeBytes) {
            buffer.push_back('\n');
            buffer.push_back('\n');
            bytesGenerated += 2;
        }
    }
    
    // Upewnij się, że mamy dokładnie targetSizeBytes
    if (buffer.size() > targetSizeBytes) {
        buffer.resize(targetSizeBytes);
    }
    
    seed = localSeed;
}
