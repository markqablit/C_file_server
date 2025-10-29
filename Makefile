CC=gcc 
SOURCES = main.c str.c 
TARGET = server.exe

$(TARGET): main.o str.o
	$(CC) $^ -o $@
main.o: main.c
	$(CC) -c $^ -o $@
str.o: str.c
	$(CC) -c $^ -o $@
clear:
	rm -f *.o $(TARGET)