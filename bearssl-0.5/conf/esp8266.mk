# Example configuration file for compiling for an ESP8266,
# on a Unix-like system, with a GNU toolchain.

# We are on a Unix system so we assume a Single Unix compatible 'make'
# utility, and Unix defaults.
include conf/Unix.mk

# We override the build directory.
BUILD = esp8266

# C compiler, linker, and static library builder.
include ../config.mk
CC = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-gcc
CFLAGS = -W -Wall -Os -mlongcalls -ffunction-sections -fdata-sections
LD = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-ld
AR = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-ar

# We compile only the static library.
DLL = no
TOOLS = no
TESTS = no
