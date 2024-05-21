CC = gcc
SRC = src/assistant-button.c
TARGET = assistant-button

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
