CC = gcc

CFLAGS = `pkg-config --cflags gio-2.0 gstreamer-1.0 dbus-1` -Iinclude
LDFLAGS = `pkg-config --libs gio-2.0 gstreamer-1.0 dbus-1` -lbatman-wrappers -lwayland-client -lxkbcommon

SOURCES = src/assistant-button.c src/actions.c src/utils.c src/virtual-keyboard-unstable-v1-protocol.c src/virtkey.c src/dbus.c src/config.c

TARGET = assistant-button

PREFIX ?= /usr

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(SOURCES) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/libexec
	install -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/libexec/
	install -d $(DESTDIR)$(PREFIX)/lib/systemd/user
	install -m 0644 data/assistant-button.service $(DESTDIR)$(PREFIX)/lib/systemd/user/
	install -d $(DESTDIR)$(PREFIX)/share/assistant-button
	install -m 0644 data/assistant-button.conf $(DESTDIR)$(PREFIX)/share/assistant-button/

clean:
	rm -f $(TARGET)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/libexec/$(TARGET)
	rm -f $(DESTDIR)$(PREFIX)/lib/systemd/user/assistant-button.service
	rm -rf $(DESTDIR)$(PREFIX)/share/assistant-button/
