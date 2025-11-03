CC=gcc 
SOURCES = main.c str.c 
TARGET = server.exe

$(TARGET): main.o str.o dir.o
	$(CC) $^ -o $@

main.o: main.c 
	$(CC) -c $^ -o $@

str.o: str.c 
	$(CC) -c $^ -o $@

dir.o: dir.c
	$(CC) -c $^ -o $@

test.exe: str.o test.o
	$(CC) $^ -o $@

test.o: test.c 
	$(CC) -c $^ -o $@

clean:
	rm -f *.o $(TARGET) test.exe