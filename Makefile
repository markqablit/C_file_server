CC = gcc 
LIBS = -lws2_32
BUILD_DIR = .\build
SOURCES_DIR = .\src
HEADER_DIR = .\include
SOURCES = main.c str.c dir.c
TARGET = server.exe

build\$(TARGET): build\main.o build\str.o build\dir.o 
	$(CC) $^ -o $@ $(LIBS)

build\main.o: src\main.c 
	$(CC) -c $^ -o $@ -I$(HEADER_DIR)

build\str.o: src\str.c 
	$(CC) -c $^ -o $@ -I$(HEADER_DIR)

build\dir.o: src\dir.c
	$(CC) -c $^ -o $@ -I$(HEADER_DIR)

tests\test.exe: build\str.o tests\test.o
	$(CC) $^ -o $@ $(LIBS) 

tests\test.o: tests\test.c 
	$(CC) -c $^ -o $@ -I$(HEADER_DIR) 

clean:
	rm -f build\main.o build\str.o build\dir.o test\test.o build\$(TARGET) tests\test.exe
.PHONY: clean