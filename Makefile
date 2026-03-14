# Makefile for DashScope SDK C
#
# Build all example programs and run them

CC := gcc
CFLAGS := -Wall -Wextra -std=c99
LDFLAGS := -lcurl -lcjson

TARGETS := text_input image_input video_input audio_input function_call

.PHONY: all clean run help

# Default target: build all examples
all: $(TARGETS)

# Build individual example programs
text_input: text_input.c dashscope.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

image_input: image_input.c dashscope.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

video_input: video_input.c dashscope.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

audio_input: audio_input.c dashscope.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

function_call: function_call.c dashscope.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Run a specific example: make run EXAMPLE=text_input
run: all
	@if [ -z "$(EXAMPLE)" ]; then \
		echo "Usage: make run EXAMPLE=<target>"; \
		echo "Available examples: $(TARGETS)"; \
	else \
		./$(EXAMPLE); \
	fi

# Clean build artifacts
clean:
	rm -f $(TARGETS) *.o

# Show help
help:
	@echo "DashScope SDK C - Makefile targets:"
	@echo "  make              - Build all examples"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make run EXAMPLE=<name>  - Run a specific example"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Available examples: $(TARGETS)"
	@echo ""
	@echo "Examples:"
	@echo "  make"
	@echo "  make run EXAMPLE=text_input"
	@echo "  make clean"
