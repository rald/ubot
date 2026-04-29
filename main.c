#define IRC_IMPLEMENTATION
#include "uirc.h"

#define DEFAULT_HOST "irc.undernet.org"
#define DEFAULT_PORT "6667"
#define DEFAULT_CHAN "#pantasya"
#define DEFAULT_NICK "friu"

int main() {
    uirc_t uirc;
    char line[1024];
    char nick[32] = DEFAULT_NICK;

    printf("Connecting to %s:%s...\n", DEFAULT_HOST, DEFAULT_PORT);

    if (uirc_connect(&uirc, DEFAULT_HOST, DEFAULT_PORT) < 0) {
        perror("Connection failed");
        return 1;
    }

    uirc_send(&uirc, "NICK %s", nick);
    uirc_send(&uirc, "USER %s 0 * :%s", nick, nick);

    while (uirc_recv_line(&uirc, line, sizeof(line)) > 0) {
        if (strncmp(line, "PING ", 5) == 0) {
            uirc_send(&uirc, "PONG %s", line + 5);
            continue;
        }

        printf("<<< %s", line);

        char *cmd = (line[0] == ':') ? strchr(line, ' ') : line;
        if (!cmd) continue;
        if (cmd != line) cmd++;

        if (strncmp(cmd, "433", 3) == 0) {
            strncat(nick, "_", sizeof(nick) - strlen(nick) - 1);
            uirc_send(&uirc, "NICK %s", nick);
        }

        if (strncmp(cmd, "376", 3) == 0 || strncmp(cmd, "422", 3) == 0) {
            uirc_send(&uirc, "JOIN %s", DEFAULT_CHAN);
        }

        if (strncmp(cmd, "PRIVMSG", 7) == 0 && strstr(cmd, "!info")) {
            uirc_send(&uirc, "PRIVMSG %s :Optimized C bot active.", DEFAULT_CHAN);
        }
    }

    printf("Connection lost.\n");
    uirc_close(&uirc);
    return 0;
}