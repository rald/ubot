#define IRC_IMPLEMENTATION
#include "uirc.h"

// Connection Configuration
#define DEFAULT_HOST "irc.undernet.org"
#define DEFAULT_PORT "6667"
#define DEFAULT_CHAN "#pantasya"

int main() {
    irc_t irc;
    char line[4096];
    char nick[32] = "friu";

    printf("Connecting to %s:%s...\n", DEFAULT_HOST, DEFAULT_PORT);
    
    // 1. Establish connection (Portable: handles Winsock on Windows or POSIX on Unix)
    if (irc_connect(&irc, DEFAULT_HOST, DEFAULT_PORT) < 0) {
        perror("Connection failed");
        return 1;
    }

    // 2. Identify to the server
    // USER syntax: USER <username> <mode> <unused> :<realname>
    irc_send(&irc, "NICK %s", nick);
    irc_send(&irc, "USER %s 0 * :%s", nick, nick);

    // 3. Main event loop
    // irc_recv_line handles the partial TCP stream and returns one full line at a time
    while (irc_recv_line(&irc, line, sizeof(line)) > 0) {
        // Print raw output for debugging
        printf("<<< %s", line);

        // --- Logic: Handle Nickname in Use (Error 433) ---
        if (strstr(line, " 433 ")) {
            if (strlen(nick) < sizeof(nick) - 2) {
                strcat(nick, "_");
                printf("!!! Nickname taken, retrying as: %s\n", nick);
                irc_send(&irc, "NICK %s", nick);
            } else {
                fprintf(stderr, "Fatal: Nickname limit reached.\n");
                break;
            }
        }

        // --- Logic: Respond to PING (Keep-alive) ---
        // Protocol requires PONG <daemon> to avoid timeout
        if (strncmp(line, "PING", 4) == 0) {
            line[1] = 'O'; // Quick swap: PING -> PONG
            irc_send(&irc, "%s", line);
        }

        // --- Logic: Join Channel after Welcome (376 = End of MOTD, 422 = No MOTD) ---
        if (strstr(line, " 376 ") || strstr(line, " 422 ")) {
            irc_send(&irc, "JOIN %s", DEFAULT_CHAN);
        }

        // --- Logic: Basic Auto-Responder ---
        if (strstr(line, "PRIVMSG") && strstr(line, "!info")) {
            irc_send(&irc, "PRIVMSG %s :I am a minimal portable C bot.", DEFAULT_CHAN);
        }
    }

    // 4. Cleanup
    printf("Connection lost.\n");
    irc_close(&irc);
    return 0;
}