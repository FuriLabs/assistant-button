CC = gcc
CFLAGS = `pkg-config --cflags gio-2.0 gstreamer-1.0 dbus-1`
LDFLAGS = `pkg-config --libs gio-2.0 gstreamer-1.0 dbus-1` -lbatman-wrappers -lwayland-client -lxkbcommon
SRC = src/assistant-button.c src/actions.c src/utils.c src/virtual-keyboard-unstable-v1-protocol.c src/virtkey.c
TARGET = assistant-button

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET)
