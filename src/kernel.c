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

/* --- Globals --- */
static volatile uint32_t timer_ticks = 0;
#define MAX_CMD_LEN 128
static char cmd_buffer[MAX_CMD_LEN];

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
    term_print("             miniOS v0.3.0         \n\n", COLOR_CYAN_ON_BLACK);

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
        delay(500);
    }
    
    delay(5000); // Pause before clearing
    term_clear();
}

/* --- Keyboard Driver --- */
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

/* --- Input Functions --- */

uint8_t get_scancode() {
    while (!(inb(0x64) & 1)) {
        timer_ticks++;
    }
    return inb(0x60);
}

void term_readline(char* buffer, int max_len, int mask) {
    int index = 0;
    while (1) {
        uint8_t scancode = get_scancode();
        if (!(scancode & 0x80)) {
            char c = scancode_to_ascii(scancode);
            if (c == '\n') {
                term_putc('\n', COLOR_WHITE_ON_BLACK);
                buffer[index] = '\0';
                break;
            } else if (c == '\b') {
                if (index > 0) {
                    index--;
                    if (term_col > 0) {
                        term_col--;
                        const size_t idx = term_row * VGA_WIDTH + term_col;
                        term_buf[idx] = (uint16_t) ' ' | (uint16_t) COLOR_WHITE_ON_BLACK << 8;
                        update_cursor(term_col, term_row);
                    }
                }
            } else if (c && index < max_len - 1) {
                buffer[index++] = c;
                if (mask) {
                    term_putc('*', COLOR_WHITE_ON_BLACK);
                } else {
                    term_putc(c, COLOR_WHITE_ON_BLACK);
                }
            }
        }
    }
}

int login_screen() {
    char username[32];
    char password[32];
    int attempts = 0;

    while (attempts < 3) {
        term_clear();
        term_print("========================================\n", COLOR_CYAN_ON_BLACK);
        term_print("            miniOS SECURE LOGIN         \n", COLOR_CYAN_ON_BLACK);
        term_print("========================================\n\n", COLOR_CYAN_ON_BLACK);

        term_print("Username: ", COLOR_WHITE_ON_BLACK);
        term_readline(username, 32, 0);

        term_print("Password: ", COLOR_WHITE_ON_BLACK);
        term_readline(password, 32, 1);

        if (strcmp(username, "admin") == 0 && strcmp(password, "mini123") == 0) {
            term_print("\nLogin successful! Welcome, ", COLOR_GREEN_ON_BLACK);
            term_print(username, COLOR_GREEN_ON_BLACK);
            term_print("\n", COLOR_GREEN_ON_BLACK);
            delay(10000);
            return 1;
        } else {
            term_print("\nInvalid username or password.\n", COLOR_RED_ON_BLACK);
            attempts++;
            delay(15000);
        }
    }

    term_print("\nToo many failed attempts. System locked.\n", COLOR_RED_ON_BLACK);
    while(1) asm volatile("hlt");
    return 0;
}

/* --- Shell Commands --- */

uint8_t current_text_color = COLOR_WHITE_ON_BLACK;

void cmd_help() {
    term_print("Available commands:\n", COLOR_WHITE_ON_BLACK);
    term_print("  help      - Show this help message\n", COLOR_WHITE_ON_BLACK);
    term_print("  clear     - Clear the terminal\n", COLOR_WHITE_ON_BLACK);
    term_print("  ls        - List virtual directories\n", COLOR_WHITE_ON_BLACK);
    term_print("  echo      - Echo text to screen\n", COLOR_WHITE_ON_BLACK);
    term_print("  calc      - Simple calculator (e.g., calc 5 + 3)\n", COLOR_WHITE_ON_BLACK);
    term_print("  color     - Change text color (0-15)\n", COLOR_WHITE_ON_BLACK);
    term_print("  minifetch - Show system info\n", COLOR_WHITE_ON_BLACK);
    term_print("  date      - Show CMOS date and time\n", COLOR_WHITE_ON_BLACK);
    term_print("  uptime    - Show system uptime\n", COLOR_WHITE_ON_BLACK);
    term_print("  cat <file>- Read a virtual file\n", COLOR_WHITE_ON_BLACK);
    term_print("  neofetch  - Fancy system information\n", COLOR_WHITE_ON_BLACK);
    term_print("  ver       - Show kernel version\n", COLOR_WHITE_ON_BLACK);
    term_print("  whoami    - Show current user\n", COLOR_WHITE_ON_BLACK);
    term_print("  cpu       - Show CPU info via CPUID\n", COLOR_WHITE_ON_BLACK);
    term_print("  ps        - List running processes\n", COLOR_WHITE_ON_BLACK);
    term_print("  reboot    - Reboot the system\n", COLOR_WHITE_ON_BLACK);
    term_print("  shutdown  - Halt the system\n", COLOR_WHITE_ON_BLACK);
}

void cmd_calc(char* args) {
    if (!args || *args == '\0') {
        term_print("Usage: calc <num1> <op> <num2>\n", COLOR_WHITE_ON_BLACK);
        return;
    }
    int n1 = 0, n2 = 0;
    char op = 0;
    char* p = args;
    while (*p == ' ') p++;
    while (*p >= '0' && *p <= '9') {
        n1 = n1 * 10 + (*p - '0');
        p++;
    }
    while (*p == ' ') p++;
    op = *p;
    p++;
    while (*p == ' ') p++;
    while (*p >= '0' && *p <= '9') {
        n2 = n2 * 10 + (*p - '0');
        p++;
    }
    int res = 0;
    if (op == '+') res = n1 + n2;
    else if (op == '-') res = n1 - n2;
    else if (op == '*') res = n1 * n2;
    else if (op == '/') {
        if (n2 == 0) {
            term_print("Error: Division by zero\n", COLOR_RED_ON_BLACK);
            return;
        }
        res = n1 / n2;
    } else {
        term_print("Unsupported operator. Use +, -, *, /\n", COLOR_WHITE_ON_BLACK);
        return;
    }
    char buf[16];
    term_print("Result: ", COLOR_WHITE_ON_BLACK);
    itoa(res, buf, 10);
    term_print(buf, COLOR_CYAN_ON_BLACK);
    term_putc('\n', COLOR_WHITE_ON_BLACK);
}

void cmd_color(char* args) {
    if (!args || *args == '\0') {
        term_print("Usage: color <0-15>\n", COLOR_WHITE_ON_BLACK);
        return;
    }
    int c = 0;
    char* p = args;
    while (*p == ' ') p++;
    while (*p >= '0' && *p <= '9') {
        c = c * 10 + (*p - '0');
        p++;
    }
    if (c >= 0 && c <= 15) {
        current_text_color = (uint8_t)c;
        term_print("Color changed.\n", current_text_color);
    } else {
        term_print("Invalid color code. Use 0-15.\n", COLOR_RED_ON_BLACK);
    }
}

void cmd_ls(char* args) {
    if (!args || *args == '\0') {
        term_print("bin/  dev/  etc/  home/  root/  tmp/\n", current_text_color);
        return;
    }
    char* p = args;
    while (*p == ' ') p++;
    if (strcmp(p, "etc") == 0 || strcmp(p, "etc/") == 0) {
        term_print("version  motd\n", current_text_color);
    } else if (strcmp(p, "root") == 0 || strcmp(p, "root/") == 0) {
        term_print("secret.txt\n", current_text_color);
    } else {
        term_print("ls: ", COLOR_RED_ON_BLACK);
        term_print(p, COLOR_RED_ON_BLACK);
        term_print(": No such directory\n", COLOR_RED_ON_BLACK);
    }
}

void cmd_cat(char* args) {
    if (!args || *args == '\0') {
        term_print("Usage: cat <file>\n", COLOR_WHITE_ON_BLACK);
        return;
    }
    char* p = args;
    while (*p == ' ') p++;
    if (strcmp(p, "etc/version") == 0) {
        term_print("miniOS v0.3.0-stable\n", current_text_color);
    } else if (strcmp(p, "etc/motd") == 0) {
        term_print("Welcome to miniOS! The really working OS.\n", current_text_color);
    } else if (strcmp(p, "root/secret.txt") == 0) {
        term_print("The cake is a lie!\n", current_text_color);
    } else {
        term_print("cat: ", COLOR_RED_ON_BLACK);
        term_print(p, COLOR_RED_ON_BLACK);
        term_print(": No such file or directory\n", COLOR_RED_ON_BLACK);
    }
}

void cmd_ps() {
    term_print("PID  TTY      TIME     CMD\n", current_text_color);
    term_print("1    tty1     00:00:01 init\n", current_text_color);
    term_print("2    tty1     00:00:00 kthreadd\n", current_text_color);
    term_print("14   tty1     00:00:00 minishell\n", current_text_color);
}

void cmd_minifetch() {
    term_print("      .---.\n", COLOR_CYAN_ON_BLACK);
    term_print("     /  _  \\     miniOS v0.3.0\n", COLOR_CYAN_ON_BLACK);
    term_print("    |  (_)  |    CPU: x86\n", COLOR_CYAN_ON_BLACK);
    term_print("     \\  _  /     Shell: MiniShell Plus\n", COLOR_CYAN_ON_BLACK);
    term_print("      '---'      State: Really Working\n\n", COLOR_CYAN_ON_BLACK);
}

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

    second = (second & 0x0F) + ((second / 16) * 10);
    minute = (minute & 0x0F) + ((minute / 16) * 10);
    hour   = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
    day    = (day & 0x0F) + ((day / 16) * 10);
    month  = (month & 0x0F) + ((month / 16) * 10);
    year   = (year & 0x0F) + ((year / 16) * 10);

    char buf[16];
    term_print("Date: 20", current_text_color);
    itoa(year, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_putc('-', current_text_color);
    itoa(month, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_putc('-', current_text_color);
    itoa(day, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_print(" Time: ", current_text_color);
    itoa(hour, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_putc(':', current_text_color);
    itoa(minute, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_putc(':', current_text_color);
    itoa(second, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_putc('\n', current_text_color);
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
    term_print("CPU: ", current_text_color);
    term_print(brand, COLOR_CYAN_ON_BLACK);
    term_putc('\n', current_text_color);
    uint32_t edx;
    asm volatile("cpuid" : "=d"(edx) : "a"(1));
    term_print("FPU Support: ", current_text_color);
    if (edx & 1) term_print("Yes\n", COLOR_GREEN_ON_BLACK);
    else term_print("No\n", COLOR_RED_ON_BLACK);
}

void cmd_uptime() {
    uint32_t total_seconds = timer_ticks / 20000000; 
    uint32_t minutes = total_seconds / 60;
    uint32_t seconds = total_seconds % 60;
    char buf[16];
    term_print("Uptime: ", current_text_color);
    itoa(minutes, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_print("m ", current_text_color);
    itoa(seconds, buf, 10); term_print(buf, COLOR_CYAN_ON_BLACK);
    term_print("s\n", current_text_color);
}

void exec_command(char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "clear") == 0) {
        term_clear();
    } else if (strncmp(cmd, "ls", 2) == 0) {
        cmd_ls(cmd + 2);
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        term_print(cmd + 5, current_text_color);
        term_putc('\n', current_text_color);
    } else if (strncmp(cmd, "cat ", 4) == 0) {
        cmd_cat(cmd + 4);
    } else if (strncmp(cmd, "calc ", 5) == 0) {
        cmd_calc(cmd + 5);
    } else if (strncmp(cmd, "color ", 6) == 0) {
        cmd_color(cmd + 6);
    } else if (strcmp(cmd, "minifetch") == 0 || strcmp(cmd, "neofetch") == 0) {
        cmd_minifetch();
    } else if (strcmp(cmd, "date") == 0) {
        cmd_date();
    } else if (strcmp(cmd, "uptime") == 0) {
        cmd_uptime();
    } else if (strcmp(cmd, "ver") == 0) {
        term_print("miniOS Kernel v0.3.0\n  - Login system added.\n  - Enhanced shell commands.\n", current_text_color);
    } else if (strcmp(cmd, "whoami") == 0) {
        term_print("admin\n", current_text_color);
    } else if (strcmp(cmd, "ps") == 0) {
        cmd_ps();
    } else if (strcmp(cmd, "cpu") == 0) {
        cmd_cpu();
    } else if (strcmp(cmd, "reboot") == 0) {
        term_print("Rebooting...\n", current_text_color);
        uint8_t good = 0x02;
        while (good & 0x02) good = inb(0x64);
        outb(0x64, 0xFE);
    } else if (strcmp(cmd, "shutdown") == 0) {
        term_print("System halted.\n", current_text_color);
        outw(0x604, 0x2000); 
        outw(0xB004, 0x2000);
        asm volatile ("cli; hlt");
    } else if (cmd[0] != '\0') {
        term_print("Unknown command: ", current_text_color);
        term_print(cmd, current_text_color);
        term_putc('\n', current_text_color);
    }
}

void kernel_main(void) {
    boot_splash();
    if (login_screen()) {
        term_clear();
        term_print("miniOS Kernel v0.3.0 Interactive Shell\n", COLOR_WHITE_ON_BLACK);
        term_print("admin@miniOS:# ", COLOR_GREEN_ON_BLACK);
        while (1) {
            term_readline(cmd_buffer, MAX_CMD_LEN, 0);
            exec_command(cmd_buffer);
            term_print("admin@miniOS:# ", COLOR_GREEN_ON_BLACK);
        }
    }
}
