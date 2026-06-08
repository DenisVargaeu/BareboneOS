#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_CMD_LEN 256
#define MAX_ARGS 10

// ANSI Color Codes for "Hacker Terminal" feel
#define COLOR_GREEN "\033[0;32m"
#define COLOR_CYAN  "\033[0;36m"
#define COLOR_RED   "\033[0;31m"
#define COLOR_RESET "\033[0m"

static time_t boot_time;

void boot_screen() {
    printf(COLOR_CYAN);
    printf("  __  __ _       _  ____   ____  \n");
    printf(" |  \\/  (_)_ __ (_)/ __ \\ / ___| \n");
    printf(" | |\\/| | | '_ \\| | |  | |\\___ \\ \n");
    printf(" | |  | | | | | | | |__| | ___) |\n");
    printf(" |_|  |_|_|_| |_|_|\\____/ |____/ \n");
    printf("                                 \n");
    printf(COLOR_RESET);
    
    printf("Starting miniOS Kernel...\n");
    usleep(500000);
    printf("[ OK ] Initializing memory management...\n");
    usleep(300000);
    printf("[ OK ] Loading drivers...\n");
    usleep(400000);
    printf("[ OK ] Mounting virtual file system...\n");
    usleep(300000);
    printf("[ OK ] Starting Shell...\n\n");
    usleep(500000);
}

void cmd_help() {
    printf("miniOS available commands:\n");
    printf("  help      - Display this help message\n");
    printf("  clear     - Clear the terminal screen\n");
    printf("  echo [txt]- Print text to the console\n");
    printf("  time      - Show current system time\n");
    printf("  uptime    - Show system uptime\n");
    printf("  ls        - List virtual files\n");
    printf("  whoami    - Show current user\n");
    printf("  cpu       - Show CPU info\n");
    printf("  ver       - Show kernel version\n");
    printf("  minifetch - Show system info\n");
    printf("  exit      - Shut down miniOS\n");
}

void cmd_time() {
    time_t now = time(NULL);
    char *t_str = ctime(&now);
    t_str[strlen(t_str) - 1] = '\0'; // Remove newline
    printf("Current time: %s\n", t_str);
}

void cmd_uptime() {
    time_t now = time(NULL);
    double seconds = difftime(now, boot_time);
    printf("System Uptime: %.0f seconds\n", seconds);
}

void cmd_cpu() {
    FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        int count = 0;
        while (fgets(line, sizeof(line), cpuinfo) && count < 10) {
            if (strstr(line, "model name") || strstr(line, "vendor_id") || strstr(line, "cpu MHz")) {
                printf("%s", line);
                count++;
            }
        }
        fclose(cpuinfo);
    } else {
        printf("CPU: Simulated x86 Host Processor\n");
    }
}

void cmd_minifetch() {
    printf(COLOR_CYAN "      .---.\n");
    printf("     /  _  \\     miniOS v0.2.3 (Simulated)\n");
    printf("    |  (_)  |    CPU: x86_64 (Host)\n");
    printf("     \\  _  /     Shell: MiniShell\n");
    printf("      '---'      Uptime: Simulated\n\n" COLOR_RESET);
}

void cmd_ls() {
    printf("bin/  dev/  etc/  home/  root/  tmp/\n");
}

int main() {
    boot_time = time(NULL);
    char input[MAX_CMD_LEN];
    char *args[MAX_ARGS];
    
    boot_screen();

    while (1) {
        printf(COLOR_GREEN "root@miniOS" COLOR_RESET ":" COLOR_CYAN "# " COLOR_RESET);
        if (fgets(input, MAX_CMD_LEN, stdin) == NULL) {
            break;
        }

        // Remove trailing newline
        input[strcspn(input, "\n")] = 0;

        // Skip empty input
        if (strlen(input) == 0) continue;

        // Simple command parsing
        int i = 0;
        args[i] = strtok(input, " ");
        while (args[i] != NULL && i < MAX_ARGS - 1) {
            args[++i] = strtok(NULL, " ");
        }

        char *cmd = args[0];

        if (strcmp(cmd, "exit") == 0) {
            printf("Shutting down miniOS...\n");
            break;
        } else if (strcmp(cmd, "help") == 0) {
            cmd_help();
        } else if (strcmp(cmd, "clear") == 0) {
            printf("\033[H\033[J");
        } else if (strcmp(cmd, "time") == 0) {
            cmd_time();
        } else if (strcmp(cmd, "uptime") == 0) {
            cmd_uptime();
        } else if (strcmp(cmd, "ls") == 0) {
            cmd_ls();
        } else if (strcmp(cmd, "whoami") == 0) {
            printf("root\n");
        } else if (strcmp(cmd, "cpu") == 0) {
            cmd_cpu();
        } else if (strcmp(cmd, "ver") == 0) {
            printf("miniOS Kernel v0.2.3 (Simulated)\n");
        } else if (strcmp(cmd, "minifetch") == 0) {
            cmd_minifetch();
        } else if (strcmp(cmd, "echo") == 0) {
            for (int j = 1; j < i; j++) {
                printf("%s ", args[j]);
            }
            printf("\n");
        } else {
            printf(COLOR_RED "Unknown command: %s" COLOR_RESET "\n", cmd);
        }
    }

    return 0;
}
