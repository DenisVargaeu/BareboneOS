#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ADDR 0xB8000

// VGA Colors
#define COLOR_WHITE_ON_BLACK 0x0F
#define COLOR_GREEN_ON_BLACK 0x0A
#define COLOR_CYAN_ON_BLACK  0x0B
#define COLOR_RED_ON_BLACK   0x0C

static size_t term_row = 0;
static size_t term_col = 0;
static volatile uint16_t* term_buf = (volatile uint16_t*) VGA_ADDR;

/* --- Port I/O functions --- */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

void update_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t) (pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

/* --- Utilities --- */
void delay(int count) {
    for (int i = 0; i < count * 100000; i++) {
        asm volatile ("nop");
    }
}

/* --- Terminal functions --- */
void term_scroll() {
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            term_buf[(y-1) * VGA_WIDTH + x] = term_buf[y * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        term_buf[(VGA_HEIGHT-1) * VGA_WIDTH + x] = (uint16_t) ' ' | (uint16_t) COLOR_WHITE_ON_BLACK << 8;
    }
}

void term_putc(char c, uint8_t color) {
    if (term_row >= VGA_HEIGHT) {
        term_scroll();
        term_row = VGA_HEIGHT - 1;
    }

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
        term_scroll();
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

void boot_splash() {
    term_clear();

    // ASCII Logo in CYAN
    term_print("  __  __ _       _  ____   ____  \n", COLOR_CYAN_ON_BLACK);
    term_print(" |  \\/  (_)_ __ (_)/ __ \\ / ___| \n", COLOR_CYAN_ON_BLACK);
    term_print(" | |\\/| | | '_ \\| | |  | |\\___ \\ \n", COLOR_CYAN_ON_BLACK);
    term_print(" | |  | | | | | | | |__| | ___) |\n", COLOR_CYAN_ON_BLACK);
    term_print(" |_|  |_|_|_| |_|_|\\____/ |____/ \n", COLOR_CYAN_ON_BLACK);
    term_print("             miniOS v0.2         \n\n", COLOR_CYAN_ON_BLACK);

    // Centered Loading Bar Configuration
    int bar_width = 40;
    int start_col = (VGA_WIDTH - bar_width) / 2;
    int row = 18;

    // Draw label
    term_row = row - 1;
    term_col = (VGA_WIDTH - 14) / 2;
    term_print("Loading System", COLOR_WHITE_ON_BLACK);

    // Draw frame
    term_row = row;
    term_col = start_col;
    term_putc('[', COLOR_WHITE_ON_BLACK);
    term_col = start_col + bar_width - 1;
    term_putc(']', COLOR_WHITE_ON_BLACK);

    // Fill with animation
    for (int i = 0; i < bar_width - 2; i++) {
        term_row = row;
        term_col = start_col + 1 + i;
        term_putc('#', COLOR_CYAN_ON_BLACK);
        delay(50);
    }
    delay(500); // Pause before clearing
    term_clear();
}

/* --- Keyboard Driver (Polling) --- */
char scancode_to_ascii(uint8_t scancode) {
    static const char kbd_map[] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
        '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
        0, // 58: Caps Lock
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 59-68: F1-F10
        0, // 69: Num Lock
        0, // 70: Scroll Lock
        '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.' // 71-83: Keypad
    };
    if (scancode < sizeof(kbd_map)) return kbd_map[scancode];
    return 0;
}

#define MAX_CMD_LEN 128
static char cmd_buffer[MAX_CMD_LEN];
static int cmd_index = 0;

/* --- String Utilities --- */
void itoa(uint32_t n, char* s, int base) {
    int i = 0;
    if (n == 0) {
        s[i++] = '0';
        s[i] = '\0';
        return;
    }
    while (n > 0) {
        int rem = n % base;
        s[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        n = n / base;
    }
    s[i] = '\0';
    // Reverse
    for (int j = 0; j < i / 2; j++) {
        char temp = s[j];
        s[j] = s[i - j - 1];
        s[i - j - 1] = temp;
    }
}

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

void cmd_help() {
    term_print("Available commands:\n", COLOR_WHITE_ON_BLACK);
    term_print("  help      - Show this help message\n", COLOR_WHITE_ON_BLACK);
    term_print("  clear     - Clear the terminal\n", COLOR_WHITE_ON_BLACK);
    term_print("  ls        - List files\n", COLOR_WHITE_ON_BLACK);
    term_print("  echo      - Echo text\n", COLOR_WHITE_ON_BLACK);
    term_print("  minifetch - Show system info\n", COLOR_WHITE_ON_BLACK);
    term_print("  date      - Show date\n", COLOR_WHITE_ON_BLACK);
    term_print("  uptime    - Show system uptime\n", COLOR_WHITE_ON_BLACK);
    term_print("  ver       - Show kernel version\n", COLOR_WHITE_ON_BLACK);
    term_print("  whoami    - Show current user\n", COLOR_WHITE_ON_BLACK);
    term_print("  cpu       - Show CPU info\n", COLOR_WHITE_ON_BLACK);
    term_print("  reboot    - Reboot the system\n", COLOR_WHITE_ON_BLACK);
    term_print("  shutdown  - Halt the system\n", COLOR_WHITE_ON_BLACK);
}

void cmd_minifetch() {
    term_print("      .---.\n", COLOR_CYAN_ON_BLACK);
    term_print("     /  _  \\     miniOS v0.2.1\n", COLOR_CYAN_ON_BLACK);
    term_print("    |  (_)  |    CPU: x86\n", COLOR_CYAN_ON_BLACK);
    term_print("     \\  _  /     Shell: MiniShell\n", COLOR_CYAN_ON_BLACK);
    term_print("      '---'      Uptime: 1m (estimated)\n\n", COLOR_CYAN_ON_BLACK);
}

static volatile uint32_t timer_ticks = 0;

/* --- Real Hardware Commands --- */

uint8_t get_rtc_register(int reg) {
    outb(0x70, reg);
    return inb(0x71);
}

int rtc_is_updating() {
    outb(0x70, 0x0A);
    return (inb(0x71) & 0x80);
}

void cmd_date() {
    while (rtc_is_updating());
    uint8_t second = get_rtc_register(0x00);
    uint8_t minute = get_rtc_register(0x02);
    uint8_t hour   = get_rtc_register(0x04);
    uint8_t day    = get_rtc_register(0x07);
    uint8_t month  = get_rtc_register(0x08);
    uint8_t year   = get_rtc_register(0x09);

    // BCD Conversion
    second = (second & 0x0F) + ((second / 16) * 10);
    minute = (minute & 0x0F) + ((minute / 16) * 10);
    hour   = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
    day    = (day & 0x0F) + ((day / 16) * 10);
    month  = (month & 0x0F) + ((month / 16) * 10);
    year   = (year & 0x0F) + ((year / 16) * 10);

    char buf[16];
    term_print("Date: 20", COLOR_WHITE_ON_BLACK);
    itoa(year, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_putc('-', COLOR_WHITE_ON_BLACK);
    itoa(month, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_putc('-', COLOR_WHITE_ON_BLACK);
    itoa(day, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_print(" Time: ", COLOR_WHITE_ON_BLACK);
    itoa(hour, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_putc(':', COLOR_WHITE_ON_BLACK);
    itoa(minute, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_putc(':', COLOR_WHITE_ON_BLACK);
    itoa(second, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_putc('\n', COLOR_WHITE_ON_BLACK);
}

void get_cpu_brand(char* brand) {
    uint32_t* b = (uint32_t*)brand;
    for (int i = 0; i < 3; i++) {
        asm volatile("cpuid" : "=a"(b[i*4]), "=b"(b[i*4+1]), "=c"(b[i*4+2]), "=d"(b[i*4+3]) : "a"(0x80000002 + i));
    }
    brand[48] = '\0';
}

void cmd_cpu() {
    char brand[49];
    get_cpu_brand(brand);
    term_print("CPU: ", COLOR_WHITE_ON_BLACK);
    term_print(brand, COLOR_CYAN_ON_BLACK);
    term_putc('\n', COLOR_WHITE_ON_BLACK);
    
    uint32_t edx;
    asm volatile("cpuid" : "=d"(edx) : "a"(1));
    term_print("FPU Support: ", COLOR_WHITE_ON_BLACK);
    if (edx & 1) term_print("Yes\n", COLOR_GREEN_ON_BLACK);
    else term_print("No\n", COLOR_RED_ON_BLACK);
}

void cmd_uptime() {
    // Approx scaling for the polling loop (adjust based on hardware/emulation speed)
    uint32_t total_seconds = timer_ticks / 20000000; 
    uint32_t minutes = total_seconds / 60;
    uint32_t seconds = total_seconds % 60;
    
    char buf[16];
    term_print("Uptime: ", COLOR_WHITE_ON_BLACK);
    itoa(minutes, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_print("m ", COLOR_WHITE_ON_BLACK);
    itoa(seconds, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_print("s\n", COLOR_WHITE_ON_BLACK);
}

void exec_command(char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "clear") == 0) {
        term_clear();
    } else if (strcmp(cmd, "ls") == 0) {
        term_print("bin/  dev/  etc/  home/  root/  tmp/\n", COLOR_WHITE_ON_BLACK);
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        term_print(cmd + 5, COLOR_WHITE_ON_BLACK);
        term_putc('\n', COLOR_WHITE_ON_BLACK);
    } else if (strcmp(cmd, "minifetch") == 0) {
        cmd_minifetch();
    } else if (strcmp(cmd, "date") == 0) {
        cmd_date();
    } else if (strcmp(cmd, "uptime") == 0) {
        cmd_uptime();
    } else if (strcmp(cmd, "ver") == 0) {
        term_print("miniOS Kernel v0.2.4\n  - True hardware date (CMOS) and CPU info.\n", COLOR_WHITE_ON_BLACK);
    } else if (strcmp(cmd, "whoami") == 0) {
        term_print("root\n", COLOR_WHITE_ON_BLACK);
    } else if (strcmp(cmd, "cpu") == 0) {
        cmd_cpu();
    } else if (strcmp(cmd, "reboot") == 0) {
        term_print("Rebooting...\n", COLOR_WHITE_ON_BLACK);
        uint8_t good = 0x02;
        while (good & 0x02) good = inb(0x64);
        outb(0x64, 0xFE);
    } else if (strcmp(cmd, "shutdown") == 0) {
        term_print("System halted.\n", COLOR_WHITE_ON_BLACK);
        // Try QEMU shutdown
        outw(0x604, 0x2000);  // Newer QEMU
        outw(0xB004, 0x2000); // Older QEMU
        asm volatile ("cli; hlt");
    } else if (cmd[0] != '\0') {
        term_print("Unknown command: ", COLOR_WHITE_ON_BLACK);
        term_print(cmd, COLOR_WHITE_ON_BLACK);
        term_putc('\n', COLOR_WHITE_ON_BLACK);
    }
}

void kernel_main(void) {
    boot_splash();
    term_print("miniOS Kernel v0.2.4 Interactive Shell\n", COLOR_WHITE_ON_BLACK);
    term_print("root@miniOS:# ", COLOR_GREEN_ON_BLACK);

    while (1) {
        timer_ticks++;
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
