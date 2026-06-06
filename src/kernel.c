#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ADDR 0xB8000

// VGA Colors
#define COLOR_WHITE_ON_BLACK 0x0F
#define COLOR_GREEN_ON_BLACK 0x0A

static size_t term_row = 0;
static size_t term_col = 0;
static uint16_t* term_buf = (uint16_t*) VGA_ADDR;

/* --- Port I/O functions --- */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* --- Terminal functions --- */
void term_putc(char c, uint8_t color) {
    if (c == '\n') {
        term_col = 0;
        term_row++;
    } else {
        const size_t index = term_row * VGA_WIDTH + term_col;
        term_buf[index] = (uint16_t) c | (uint16_t) color << 8;
        term_col++;
    }

    if (term_col >= VGA_WIDTH) {
        term_col = 0;
        term_row++;
    }

    if (term_row >= VGA_HEIGHT) {
        // Simple scroll: move everything up one line
        for (size_t y = 1; y < VGA_HEIGHT; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                term_buf[(y-1) * VGA_WIDTH + x] = term_buf[y * VGA_WIDTH + x];
            }
        }
        // Clear last line
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            term_buf[(VGA_HEIGHT-1) * VGA_WIDTH + x] = (uint16_t) ' ' | (uint16_t) COLOR_WHITE_ON_BLACK << 8;
        }
        term_row = VGA_HEIGHT - 1;
    }
}

void term_print(const char* str, uint8_t color) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        term_putc(str[i], color);
    }
}

void term_clear() {
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        term_buf[i] = (uint16_t) ' ' | (uint16_t) COLOR_WHITE_ON_BLACK << 8;
    }
    term_row = 0;
    term_col = 0;
}

/* --- Keyboard Driver (Polling) --- */
char scancode_to_ascii(uint8_t scancode) {
    static const char kbd_map[] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
        '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
    };
    if (scancode < sizeof(kbd_map)) return kbd_map[scancode];
    return 0;
}

void kernel_main(void) {
    term_clear();
    term_print("miniOS Kernel v0.1 Interactive Shell\n", COLOR_WHITE_ON_BLACK);
    term_print("root@miniOS:# ", COLOR_GREEN_ON_BLACK);

    while (1) {
        // Poll for keyboard input (Status register bit 0 must be 1)
        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            
            // If bit 7 is clear, it's a key press (not release)
            if (!(scancode & 0x80)) {
                char c = scancode_to_ascii(scancode);
                if (c) {
                    term_putc(c, COLOR_WHITE_ON_BLACK);
                    if (c == '\n') {
                        term_print("root@miniOS:# ", COLOR_GREEN_ON_BLACK);
                    }
                }
            }
        }
    }
}
