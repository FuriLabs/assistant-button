CC = gcc
CFLAGS = `pkg-config --cflags gio-2.0 gstreamer-1.0`
LDFLAGS = `pkg-config --libs gio-2.0 gstreamer-1.0` -lbatman-wrappers -lwayland-client
SRC = src/assistant-button.c src/actions.c src/utils.c
TARGET = assistant-button

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET)
