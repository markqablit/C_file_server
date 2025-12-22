CC = gcc 
DEBUG = -DLOGLVL=3
LIBS = -lws2_32 -Wall
BUILD_DIR = build
SOURCES_DIR = src
HEADER_DIR = include
SOURCES = $(SOURCES_DIR)/main.c $(SOURCES_DIR)/str.c $(SOURCES_DIR)/dir.c $(SOURCES_DIR)/arg_parser.c $(SOURCES_DIR)/server_tool.c
OBJECTS = $(SOURCES:$(SOURCES_DIR)/%.c=$(BUILD_DIR)/%.o)
TARGET = $(BUILD_DIR)/server.exe

$(TARGET): $(OBJECTS)
	mkdir -p $(BUILD_DIR) 
	$(CC) $^ -o $@ $(LIBS)

%(BUILD_DIR)/server_tool.o: $(SOURCES_DIR)/server_tool.c
	$(CC) -c $^ -o $@ -I$(HEADER_DIR) $(LIBS)

$(BUILD_DIR)/%.o: $(SOURCES_DIR)/%.c
	$(CC) -c $^ -o $@ -I$(HEADER_DIR) $(DEBUG)

debug: $(OBJECTS)
	mkdir -p $(BUILD_DIR)
	$(CC) $^ -o $(TARGET) $(LIBS) -DLOGLVL=3

tests\test.exe: tests\test.o
	$(CC) $^ -o $@ $(LIBS) 

tests\test.o: tests\test.c 
	$(CC) -c $^ -o $@ -I$(HEADER_DIR) 

clean:
	rm -f $(OBJECTS) test/test.o $(TARGET) tests/test.exe
	
.PHONY: clean debug