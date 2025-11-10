# Makefile for Dart project

# Define the target executable name
TARGET = asd

# Define the source Dart file
SOURCE = bin/downloader.dart

# Default target
all: $(TARGET)

# Get dependencies
get:
	dart pub get

# Compile the Dart file to an executable
$(TARGET): get
	dart compile exe $(SOURCE) -o $@

# Clean up generated files
clean:
	rm -f $(TARGET)

# Phony targets
.PHONY: all get clean
