CC := gcc
CFLAGS := -Wall -Wextra -std=c99
LDFLAGS := -lraylib -lm -lpthread -ldl -lGL -lrt -lX11

SRC := pong_client.c
OUT := pong_client

.PHONY: all clean run

all: $(OUT)

$(OUT): $(SRC)
	@echo "Compiling $(OUT)..."
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Build finished."

run: $(OUT)
	@./$(OUT) 127.0.0.1 1

clean:
	@echo "Cleaning up..."
	@rm -f $(OUT)
