#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#define PORT 9871
#define HOST "127.0.0.1"

#define SHM_KEY 4567
#define SHM_SIZE 16384
#define SHM_PERM (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)

typedef char string[255];

typedef struct User {
    string name;
    string password;
    unsigned int points;
} User;

typedef struct Users {
    User users[10];
    unsigned int nbUsers;
} Users;

typedef struct Session {
    User users[10];
    string name;
    unsigned int nbUsers;
    unsigned int minUser;
    unsigned int col;
    unsigned int row;
} Session;

typedef struct Game {
    Session session;
    Users users;
} Game;

enum {
    GRID_WATER,
    GRID_BOAT
};

enum {
    MSG_BASE,
    MSG_NAME,
    MSG_ROW,
    MSG_COL,
    MSG_MIN_PLAYER,
    MSG_PLACE_BOAT,
    MSG_PLAY,
};

unsigned int **grid;
char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

bool isClientAdmin() {
    return true;
}

void sendPromptToClient(int clientSocket, string message) {
    uint32_t dataLen = strlen(message);
    uint32_t hostToNetInt = htonl(dataLen);
    send(clientSocket, &hostToNetInt, sizeof(hostToNetInt), 0);
    send(clientSocket, message, dataLen, 0);
}

void handleClient(int clientSocket, int client, unsigned int fd, Game *game) {
    char data[500] = {0};
    unsigned int length = 0;
    ssize_t resSize = 0;
    char *helpCommands = "Here is the list of commands:\n"
    "- addUser: <name> <password> (admin)\n"
    "- remove: remove a user (admin)\n"
    "- login: <user>\n"
    "- list: list all users\n"
    "- create: create a new game (admin)\n"
    "- quit: quit the server (admin)\n"
    "- help: list all commands\n";
    char *helpCommands2 = "Here is the list of commands:";

    uint32_t dataLen = strlen(helpCommands);
    uint32_t hostToNetInt = htonl(dataLen);
    send(clientSocket, &hostToNetInt, sizeof(hostToNetInt), 0);
    send(clientSocket, helpCommands, dataLen, 0);
    char *pos;
    string promptClient;
    // string promptClient2;
    unsigned int gameMode = MSG_BASE;
    do
    {
        switch (gameMode)
        {
            case MSG_NAME:
                strcpy(promptClient, "Session name:");
                break;
            case MSG_ROW:
                strcpy(promptClient, "Row amount:");
                break;
            case MSG_COL:
                strcpy(promptClient, "Col amount:");
                break;
            case MSG_MIN_PLAYER:
                strcpy(promptClient, "Minimum player amount:");
                break;
            case MSG_PLACE_BOAT:
                strcpy(promptClient, "");
                if (grid == NULL) {
                    grid = (unsigned int**) calloc(sizeof(unsigned int*), game->session.row);
                    if (grid == NULL) {
                        perror("malloc");
                        exit(1);
                    }
                    for (int i = 0; i < game->session.col; i++)
                    {
                        grid[i] = (unsigned int*) calloc(sizeof(unsigned int), game->session.col);
                    }
                }
                for (int i = 0; i <= game->session.col; i++)
                {
                    sprintf(promptClient, "%s%d ", promptClient, i);
                }
                strcat(promptClient, "\n");

                for (int i = 0; i < game->session.row; i++)
                {
                    sprintf(promptClient, "%s%c ", promptClient, alphabet[i]);
                    for (int j = 0; j < game->session.col; j++)
                    {
                        sprintf(promptClient, "%s%d ", promptClient, grid[j][i]);
                    }
                    strcat(promptClient, "\n");
                }

                sprintf(promptClient, "%s\nChoisir une position (ex B1) (OK pour terminer)", promptClient);
                puts(promptClient);                                
                break;
            case MSG_PLAY:
                strcpy(promptClient, "Choisir une position (ex B1)");
                break;
            
            default:
                strcpy(promptClient, "Enter a command:");
                break;
        }
        sendPromptToClient(clientSocket, promptClient);
        memset(data, 0, 500);

        puts("Waiting for client...");
        if (resSize = recv(clientSocket, data, 500, 0), resSize == -1) {
            perror("bye");
            exit(1);
        };

        if (gameMode != MSG_BASE) {
            switch (gameMode)
            {
                case MSG_NAME:
                    puts(data);
                    strcpy(game->session.name, data);
                    gameMode = MSG_ROW;
                    break;
                case MSG_ROW:
                    puts(data);
                    game->session.row = atoi(data); //check if it's a number
                    gameMode = MSG_COL;
                    break;
                case MSG_COL:
                    puts(data);
                    game->session.col = atoi(data); //check
                    gameMode = MSG_MIN_PLAYER;
                    break;
                case MSG_MIN_PLAYER:
                    puts(data);
                    game->session.minUser = atoi(data); //check
                    gameMode = MSG_PLACE_BOAT;
                    break;
                case MSG_PLACE_BOAT:
                    if (strcmp(data, "OK") == 0) {
                        // gameMode = MSG_BASE; TODO
                        break;
                    }
                    int number = 0;
                    char letter = 0;
                    sscanf(data, "%c%d", &letter, &number);
                    if (number <= 0 || number > game->session.col || letter < 'A' || letter > 'Z') {
                        puts("Invalid position");
                        break;
                    }

                    grid[number - 1][letter - 'A'] = GRID_BOAT;
                    puts(data);
                    break;

                default:
                    puts(data);
                    break;
            }
            hostToNetInt = htonl(0);
            send(clientSocket, &hostToNetInt, sizeof(hostToNetInt), 0);
            continue;
        }

        if (strcmp(data, "exit") == 0) {
            puts("fin du programme");
            exit(0);
        } else if ((pos = strstr(data, "login")), pos != NULL) {
            // check if the user is already logged
            User user;
            unsigned int n = 0;
            const char *separators = " ";
            char *elem = strtok(pos, separators);
            while (elem != NULL)
            {
                printf("%d %s\n", n, elem);
                if (n == 1) {
                    strcpy(user.name, elem);
                    printf("copy name %s\n", user.name);
                } else if (n == 2) {
                    strcpy(user.password, elem);
                    printf("copy pwd %s\n", user.password);
                }
                n++;
                elem = strtok(NULL, separators);
            }

            char *connectionFailed = "Connection failed\n";
            char *connectionSuccess = "Connection success\n";
            bool found = false;
            printf("nb users = %d\n", game->users.nbUsers);
            for (int i = 0; i < game->users.nbUsers; i++)
            {
                User *user2 = &game->users.users[i];
                puts(user2->name);
                if ((strcmp(user2->name, user.name) == 0) && (strcmp(user2->password, user.password) == 0)) {
                    dataLen = strlen(connectionSuccess);
                    hostToNetInt = htonl(dataLen);
                    send(clientSocket, &hostToNetInt, sizeof(hostToNetInt), 0);
                    send(clientSocket, connectionSuccess, dataLen, 0);
                    found = true;
                    break;
                }
            }
            if (!found) {
                dataLen = strlen(connectionFailed);
                hostToNetInt = htonl(dataLen);
                send(clientSocket, &hostToNetInt, sizeof(hostToNetInt), 0);
                send(clientSocket, connectionFailed, dataLen, 0);
            }
            continue;
        } else if (strcmp(data, "startgame") == 0) {
            gameMode = MSG_NAME;
            // check if user is admin with id using Shared Memory
        } else if ((pos = strstr(data, "remove")), pos != NULL) {
            // check if user is admin with id using Shared Memory
            pos = strchr(pos, ' ');
            if (pos == NULL) {
                puts("Invalid command");
                break;
            }
            pos++;
            printf("The user to remove is %s\n", pos);
            for (int i = 0; i < game->users.nbUsers; i++)
            {
                User *user = &game->users.users[i];
                if (strcmp(user->name, pos) == 0) {
                    game->users.nbUsers--;
                    strcpy(user->name, "");
                    strcpy(user->password, "");
                    break;
                }
            }
        } else if ((pos = strstr(data, "addUser")), pos != NULL) {
            User user;
            unsigned int n = 0;
            const char *separators = " ";
            char *elem = strtok(pos, separators);
            while (elem != NULL)
            {
                printf("%d %s\n", n, elem);
                if (n == 1) {
                    strcpy(user.name, elem);
                    printf("copy name %s\n", user.name);
                } else if (n == 2) {
                    strcpy(user.password, elem);
                    printf("copy pwd %s\n", user.password);
                }
                n++;
                elem = strtok(NULL, separators);
            }
            printf("name = %s password = %s n = %d", user.name, user.password, n);
            strcpy(game->users.users[game->users.nbUsers].name, user.name);
            strcpy(game->users.users[game->users.nbUsers].password, user.password);
            game->users.users[game->users.nbUsers].points = 0;
            game->users.nbUsers++;
        } else if (strcmp(data, "list") == 0) {
            User *user;
            size_t playerListSize = game->users.nbUsers * 255;
            char playerList[playerListSize];
            memset(playerList, '\0', playerListSize);
            for (int i = 0; i < game->users.nbUsers; i++)
            {
                user = &game->users.users[i];
                strcat(playerList, user->name);
                strcat(playerList, "\n");
            }
            dataLen = strlen(playerList);
            hostToNetInt = htonl(dataLen);
            send(clientSocket, &hostToNetInt, sizeof(hostToNetInt), 0);
            send(clientSocket, playerList, dataLen, 0);
            // write(fd, playerList, strlen(playerList));
            
            continue;
        } else if (strcmp(data, "help") == 0) {
            dataLen = strlen(helpCommands);
            hostToNetInt = htonl(dataLen);
            send(clientSocket, &hostToNetInt, sizeof(hostToNetInt), 0);
            send(clientSocket, helpCommands, dataLen, 0);
            continue;
        }

        hostToNetInt = htonl(0);
        send(clientSocket, &hostToNetInt, sizeof(hostToNetInt), 0);

        // write(fd, data, strlen(data));
    } while (1);
}

Game* createSHM() {
    return mmap(NULL, sizeof(Game), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
}

int main(int argc, int *argv[]) {
    char *logPath = "log5.txt";
    creat(logPath, O_CREAT | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
    unsigned int fd = open(logPath, O_WRONLY);
    if (fd == -1) {
        perror("Can't open log file");
        exit(1);
    }

    char *startServerMsg = "Starting server..\n";
    puts(startServerMsg);
    write(fd, startServerMsg, strlen(startServerMsg));

    void* shm = createSHM();
    if (shm == MAP_FAILED) {
        perror("Can't create shared memory");
        exit(1);
    };
    User adminUser = {
        .name = "Admin",
        .password = "ratio",
        .points = 0,
    };

    Game *game = (Game*)shm;
    game->users.users[0] = adminUser;
    game->users.nbUsers = 1;

    struct sockaddr_in srv, client;
    // AF_INET => ipv4
    // SOCK_STREAM => two-way
    int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    //check socket

    srv.sin_port = htons(PORT);
    srv.sin_addr.s_addr = inet_addr(HOST);
    srv.sin_family = AF_INET;

    // < 0 => error
    if (bind(sd, (struct sockaddr *)&srv, sizeof(struct sockaddr_in)) < 0) {
        perror("Can't bind");
        exit(1);
    }

    listen(sd, 1);

    char *serverStartedMsg = "Server started !\n";
    puts(serverStartedMsg);
    write(fd, serverStartedMsg, strlen(serverStartedMsg));
    
    int clientId = 0;
    // do
    // {
        int sz = sizeof(struct sockaddr_in);
        int clientSocket = accept(sd, (struct sockaddr *) &client, (socklen_t*) &sz);
        if (clientSocket == -1) {
            perror("rip le socket");
            exit(1);
        }
        clientId++;
    
        // int forked = fork();
        // if (forked == 0) { //handle single client
            printf("[Child] New client with id %d\n", clientId);
            handleClient(clientSocket, clientId, fd, shm);
            return EXIT_SUCCESS;
        // }
        // printf("[Main] New client with id %d\n", clientId);
    // } while (1);
    
}
