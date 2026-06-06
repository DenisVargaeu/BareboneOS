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

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void update_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t) (pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
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
        for (size_t y = 1; y < VGA_HEIGHT; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                term_buf[(y-1) * VGA_WIDTH + x] = term_buf[y * VGA_WIDTH + x];
            }
        }
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            term_buf[(VGA_HEIGHT-1) * VGA_WIDTH + x] = (uint16_t) ' ' | (uint16_t) COLOR_WHITE_ON_BLACK << 8;
        }
        term_row = VGA_HEIGHT - 1;
    }
    update_cursor(term_col, term_row);
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
    update_cursor(0, 0);
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

#define MAX_CMD_LEN 128
static char cmd_buffer[MAX_CMD_LEN];
static int cmd_index = 0;

/* --- String Utilities --- */
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

/* --- Shell Commands --- */
void exec_command(char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        term_print("Available commands: help, clear, echo [text], reboot\n", COLOR_WHITE_ON_BLACK);
    } else if (strcmp(cmd, "clear") == 0) {
        term_clear();
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        term_print(cmd + 5, COLOR_WHITE_ON_BLACK);
        term_putc('\n', COLOR_WHITE_ON_BLACK);
    } else if (strcmp(cmd, "reboot") == 0) {
        term_print("Rebooting...\n", COLOR_WHITE_ON_BLACK);
        // Standard PS/2 controller reboot hack
        uint8_t good = 0x02;
        while (good & 0x02) good = inb(0x64);
        asm volatile ("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));
    } else if (cmd[0] != '\0') {
        term_print("Unknown command: ", COLOR_WHITE_ON_BLACK);
        term_print(cmd, COLOR_WHITE_ON_BLACK);
        term_putc('\n', COLOR_WHITE_ON_BLACK);
    }
}

void kernel_main(void) {
    term_clear();
    term_print("miniOS Kernel v0.1 Interactive Shell\n", COLOR_WHITE_ON_BLACK);
    term_print("root@miniOS:# ", COLOR_GREEN_ON_BLACK);

    while (1) {
        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            if (!(scancode & 0x80)) {
                char c = scancode_to_ascii(scancode);
                if (c) {
                    if (c == '\n') {
                        term_putc('\n', COLOR_WHITE_ON_BLACK);
                        cmd_buffer[cmd_index] = '\0';
                        exec_command(cmd_buffer);
                        cmd_index = 0;
                        term_print("root@miniOS:# ", COLOR_GREEN_ON_BLACK);
                    } else if (c == '\b') {
                        if (cmd_index > 0) {
                            cmd_index--;
                            // Visual backspace
                            if (term_col > 14) { // Don't delete prompt
                                term_col--;
                                const size_t index = term_row * VGA_WIDTH + term_col;
                                term_buf[index] = (uint16_t) ' ' | (uint16_t) COLOR_WHITE_ON_BLACK << 8;
                                update_cursor(term_col, term_row);
                            }
                        }
                    } else {
                        if (cmd_index < MAX_CMD_LEN - 1) {
                            cmd_buffer[cmd_index++] = c;
                            term_putc(c, COLOR_WHITE_ON_BLACK);
                        }
                    }
                }
            }
        }
    }
}
