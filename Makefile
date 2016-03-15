HIDAPI_PROVIDER_LIBUSB := hidapi-libusb
HIDAPI_PROVIDER_HIDRAW := hidapi-hidraw
HIDAPI_PROVIDER ?= $(HIDAPI_PROVIDER_HIDRAW)

APP_NAME := missile-launcher
APP_VER := 0.1
DEVELOPER_EMAIL := frank8371@gmail.com

UDEV_RULES_MISSILE_LAUNCHER := 90-missile-launcher.rules

TARGET_APP_DIR := /usr/bin
TARGET_UDEV_RULES_DIR := /etc/udev/rules.d

DEFS := \
	-DPROGRAM_NAME="\"$(APP_NAME)\"" \
	-DPROGRAM_VERSION="\"$(APP_VER)\"" \
	-DBUG_EMAIL_ADDRESS="\"$(DEVELOPER_EMAIL)\""

CFLAGS += `pkg-config --cflags $(HIDAPI_PROVIDER)` $(DEFS) -Wall
LDLIBS += `pkg-config --libs $(HIDAPI_PROVIDER)`

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

.PHONY: all
all: $(APP_NAME)

.PHONY: clean
clean:
	rm -f $(APP_NAME) $(OBJS)

.PHONY: install-rules
install-rules:
	install $(UDEV_RULES_MISSILE_LAUNCHER) $(TARGET_UDEV_RULES_DIR)

.PHONY: install-application
install-application:
	install $(APP_NAME) $(TARGET_APP_DIR)

.PHONY: install
install: install-rules install-application

.PHONY: uninstall
uninstall:
	rm -f $(TARGET_APP_DIR)/$(APP_NAME)
	rm -f $(TARGET_UDEV_RULES_DIR)/$(UDEV_RULES_MISSILE_LAUNCHER)
