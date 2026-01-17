CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Wno-deprecated-declarations
LDFLAGS = -lssl -lcrypto

TARGET = encrypt
SOURCE = encrypt.cpp

TARGET2 = generate_ciphertexts
SOURCE2 = generate_ciphertexts.cpp

# Sprawdź czy OpenSSL jest zainstalowane
OPENSSL_CFLAGS = $(shell pkg-config --cflags openssl 2>/dev/null)
OPENSSL_LIBS = $(shell pkg-config --libs openssl 2>/dev/null)

ifndef OPENSSL_CFLAGS
    # Fallback jeśli pkg-config nie działa
    OPENSSL_CFLAGS = 
    OPENSSL_LIBS = -lssl -lcrypto
endif

all: $(TARGET) $(TARGET2)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(OPENSSL_CFLAGS) -o $(TARGET) $(SOURCE) $(OPENSSL_LIBS) $(LDFLAGS)

$(TARGET2): $(SOURCE2)
	$(CXX) $(CXXFLAGS) $(OPENSSL_CFLAGS) -o $(TARGET2) $(SOURCE2) $(OPENSSL_LIBS) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET2)

.PHONY: all clean
