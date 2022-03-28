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
    int playerId;
    bool playing;
} User;

typedef struct Users {
    User users[10];
    unsigned int nbUsers;
} Users;

typedef struct Session {
    Users users;
    string name;
    unsigned int grid[50][50];
    unsigned int minUser;
    unsigned int col;
    unsigned int row;
    unsigned int turn;
    unsigned int boatNb;
} Session;

typedef struct Game {
    Session session;
    Users users;
} Game;

enum {
    GRID_WATER,
    GRID_BOAT,
    GRID_HIT,
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

User *currentUser;
unsigned int gameMode = MSG_BASE;
char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char promptClient[1000];
Game *game; //shm

bool isClientAdmin() {
    if (currentUser == NULL) return false;
    return (strcmp(currentUser->name, "Admin") == 0);
}

bool turnForTheNextPlayer() {
    bool next = false;
    for (int i = 0; i <= game->session.users.nbUsers; i++)
    {
        printf("check next %d\n", i);
        User *user = &game->session.users.users[i];
        if (next) {
            printf("j'ai un next\n");
            printf("i = %d nb = %d\n", i, game->session.users.nbUsers);
            if (i == (game->session.users.nbUsers)) {
                printf("turn => playerId du user 0? name = %s\n", game->session.users.users[0].name);
                game->session.turn = game->session.users.users[0].playerId;
            } else {
                if (strcmp(user->name, "Admin") != 0) {
                    game->session.turn = user->playerId;
                }
            }
            printf("turn => playerId = %d\n", user->playerId);
            break;
        }
        if (user->playing && (user->playerId == game->session.turn)) {
            next = true;
        }
    }
    return next;
}

void getGrid() {
    strcpy(promptClient, "");
    for (int i = 0; i <= game->session.col; i++)
    {
        sprintf(promptClient, "%s%d ", promptClient, i);
    }
    strcat(promptClient, "\n");

    if (game->session.grid == NULL) {
        puts("grid is null");
    }

    for (int i = 0; i < game->session.row; i++)
    {
        sprintf(promptClient, "%s%c ", promptClient, alphabet[i]);
        for (int j = 0; j < game->session.col; j++)
        {
            char c = ' ';
            switch (game->session.grid[j][i])
            {
                case GRID_BOAT:
                    // if (isClientAdmin()) {
                        c = 'B';
                    // }
                    break;
                case GRID_HIT:
                    c = 'T';
                    break;
                
                default:
                    break;
            }
            sprintf(promptClient, "%s%c ", promptClient, c); //todo => switch
        }
        strcat(promptClient, "\n");
    }
    sprintf(promptClient, "%s\nChoisir une position (ex B1) (Entrée pour rafraichir la grille)", promptClient);
    if (gameMode == MSG_PLAY) {
        sprintf(promptClient, "%s Your turn = %d, Turn of %u", promptClient, currentUser->playerId, game->session.turn);
    }
}

void sendPromptToClient(int clientSocket, char *message) {
    uint32_t dataLen = strlen(message);
    uint32_t hostToNetInt = htonl(dataLen);
    send(clientSocket, &hostToNetInt, sizeof(hostToNetInt), 0);
    send(clientSocket, message, dataLen, 0);
}

void getWinner(int clientSocket) {
    puts("get winner deb");
    User *user = NULL;
    unsigned int points = 0;
    unsigned int points2 = 0;
    for (int i = 0; i < game->session.users.nbUsers; i++)
    {
        points2 = game->session.users.users[i].points;
        if (points < points2) {
            points = points2;
            user = &game->session.users.users[i];
            break;
        }
    }

    printf("user = %p", user);
    //send msg
    sprintf(promptClient, "Fin du jeu\nLe gagnant est %s avec %u points", user->name, user->points);
    sendPromptToClient(clientSocket, promptClient);
    game->session.users.nbUsers = 0;
}



void handleClient(int clientSocket, int client, unsigned int fd) {
    char data[500] = {0};
    unsigned int length = 0;
    ssize_t resSize = 0;
    char *helpCommands = "Here is the list of commands:\n"
    "- addUser <name> <password>: (admin)\n"
    "- check: check if the game started and if accepted\n"
    "- acceptuser <user>: accept a user for the session  (admin)\n"
    "- remove <user>: remove a user (admin)\n"
    "- login <user>:\n"
    "- list: list all users\n"
    "- joingame: join the game\n"
    "- userswaiting: list users waiting for a game\n"
    "- startgame: create a new game (admin)\n"
    "- quit: quit the server (admin)\n"
    "- help: list all commands\n";
    char *helpCommands2 = "Here is the list of commands:";

    uint32_t dataLen = strlen(helpCommands);
    uint32_t hostToNetInt = htonl(dataLen);
    send(clientSocket, &hostToNetInt, sizeof(hostToNetInt), 0);
    send(clientSocket, helpCommands, dataLen, 0);
    
    char *pos;
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
                getGrid();
                if (isClientAdmin()) {
                    strcat(promptClient, " (OK pour terminer)");
                }
                break;
            case MSG_PLAY:
                getGrid();
                break;
            
            default:
                strcpy(promptClient, "Enter a command:");
                break;
        }
        puts("send prompt to client");
        sendPromptToClient(clientSocket, promptClient);
        puts("send prompt to client OK");
        memset(data, 0, 500);

        // puts("Waiting for client...");
        if (resSize = recv(clientSocket, data, 500, 0), resSize == -1) {
            perror("bye");
            exit(1);
        };
        puts("recv OK");

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
                    game->session.boatNb = 0;
                    gameMode = MSG_PLACE_BOAT;
                    break;
                case MSG_PLACE_BOAT:
                    if ((strcmp(data, "OK") == 0) && (game->session.boatNb > 0)) {
                        game->session.turn = 0;
                        gameMode = MSG_BASE;
                        break;
                    }
                    int number = 0;
                    char letter = 0;
                    sscanf(data, "%c%d", &letter, &number);
                    if (number <= 0 || number > game->session.col || letter < 'A' || letter > 'Z') {
                        puts("Invalid position");
                        break;
                    }

                    game->session.grid[number - 1][letter - 'A'] = GRID_BOAT;
                    game->session.boatNb++;
                    puts(data);
                    break;
                case MSG_PLAY:
                    if (game->session.boatNb == 0) {
                        getWinner(clientSocket);
                        gameMode = MSG_BASE;
                    }
                    if (game->session.turn == currentUser->playerId && !isClientAdmin()) {
                        int number = 0;
                        char letter = 0;
                        sscanf(data, "%c%d", &letter, &number);
                        if (number <= 0 || number > game->session.col || letter < 'A' || letter > 'Z') {
                            puts("Invalid position");
                            break;
                        }
                        unsigned int *cell = &game->session.grid[number - 1][letter - 'A'];

                        switch (*cell)
                        {
                            case GRID_BOAT:
                                *cell = GRID_HIT;
                                puts("Touché");
                                game->session.boatNb--;
                                currentUser->points++;

                                for (int i = 0; i < game->session.users.nbUsers; i++)
                                {
                                    User *user = &game->session.users.users[i];
                                    if (currentUser->playerId == user->playerId) {
                                        user->points++;
                                    }
                                }
                                
                                break;
                            case GRID_WATER:
                                puts("WATER");
                                break;
                            
                            default:
                                break;
                        }

                        if (game->session.boatNb == 0) {
                            getWinner(clientSocket);
                            gameMode = MSG_BASE;
                            continue;
                        }

                        bool next = turnForTheNextPlayer();
                        printf("valeur de next = %d\n", next);
                        if (!next) {
                            printf("turn => playerId du user 0\n");
                            game->session.turn = game->session.users.users[0].playerId;
                        }
                    } else {
                        if (isClientAdmin()) {
                            bool next = turnForTheNextPlayer();
                            break;
                        }
                        if (game->session.boatNb == 0) {
                            getWinner(clientSocket);
                            gameMode = MSG_BASE;
                            continue;
                        }
                        char *msg = "It's not your turn\n";
                        sendPromptToClient(clientSocket, msg);
                        continue;
                    }
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
                    sendPromptToClient(clientSocket, connectionSuccess);
                    found = true;
                    currentUser = user2;
                    currentUser->playerId = clientSocket;
                    currentUser->playing = false;
                    break;
                }
            }
            if (!found) {
                sendPromptToClient(clientSocket, connectionFailed);
            }
            continue;
        } else if (strcmp(data, "startgame") == 0) {
            if (currentUser == NULL) {
                char *msg = "You must be logged in to start a game\n";
                sendPromptToClient(clientSocket, msg);
                continue;
            }
            if (!isClientAdmin()) {
                char *msg = "You must be an admin to start a game\n";
                sendPromptToClient(clientSocket, msg);
                continue;
            }
            gameMode = MSG_NAME;
        } else if (strcmp(data, "check") == 0) {
            int nb = 0;
            for (int i = 0; i < game->session.users.nbUsers; i++)
            {
                User *user = &game->session.users.users[i];
                if (user->playing) {
                    nb++;
                }
            }
            
            if (nb < game->session.minUser) {
                char *msg = "Not enough player to start\n";
                sendPromptToClient(clientSocket, msg);
                continue;
            }

            if (!isClientAdmin()) {
                bool ok = false;
                for (int i = 0; i < game->session.users.nbUsers; i++)
                {
                    User *user = &game->session.users.users[i];
                    if (strcmp(user->name, currentUser->name) == 0) {
                        if (game->session.turn == 0) {
                            game->session.turn = currentUser->playerId;
                        }
                        ok = true;
                        gameMode = MSG_PLAY;
                        break;
                    }
                }
                if (!ok) {
                    char *msg = "The admin didn't accepted you yet\n";
                    sendPromptToClient(clientSocket, msg);
                    continue;
                }
            } else {
                gameMode = MSG_PLAY;
            }
        } else if (strcmp(data, "userswaiting") == 0) {
            User *user;
            unsigned int nb = game->session.users.nbUsers;
            if (nb == 0) {
                char *msg = "No users waiting\n";
                sendPromptToClient(clientSocket, msg);
                continue;
            }
            size_t playerListSize = nb * 255;
            char playerList[playerListSize];
            memset(playerList, '\0', playerListSize);
            for (int i = 0; i < nb; i++)
            {
                user = &game->session.users.users[i];
                strcat(playerList, user->name);
                strcat(playerList, user->playing ? " (accepted)" : " (not accepted)");
                strcat(playerList, "\n");
            }
            sendPromptToClient(clientSocket, playerList);
            // write(fd, playerList, strlen(playerList));
            
            continue;
        } else if (strcmp(data, "joingame") == 0) {
            if (currentUser == NULL) {
                char *msg = "Not logged";
                sendPromptToClient(clientSocket, msg);
                continue;
            }

            if (strcmp(currentUser->name, "Admin") == 0) {
                char *msg = "You are admin\n";
                sendPromptToClient(clientSocket, msg);
                continue;
            }

            if (strlen(game->session.name) == 0) {
                char *msg = "No session available\n";
                sendPromptToClient(clientSocket, msg);
                continue;
            }
            //check session
            unsigned int nb = game->session.users.nbUsers;
            currentUser->playing = false;
            game->session.users.users[nb] = *currentUser;
            printf("Session user named %s with ID %d, nb = %u", currentUser->name, clientSocket, nb);
            game->session.users.users[nb].playerId = clientSocket;
            game->session.users.nbUsers++;
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
        } else if ((pos = strstr(data, "acceptuser")), pos != NULL) {
            if (!isClientAdmin()) {
                char *msg = "You must be an admin to accept a user\n";
                sendPromptToClient(clientSocket, msg);
                continue;
            }
            // check if user is admin with id using Shared Memory
            pos = strchr(pos, ' ');
            if (pos == NULL) {
                puts("Invalid command");
                break;
            }
            pos++;
            printf("The user to accept is %s\n", pos);
            for (int i = 0; i < game->session.users.nbUsers; i++)
            {
                User *user = &game->session.users.users[i];
                if (strcmp(user->name, pos) == 0) {
                    // game->session.users.nbUsers--;
                    user->playing = true;
                    break;
                }
            }
            // checkgame
        } else if ((pos = strstr(data, "addUser")), pos != NULL) {
            if (!isClientAdmin()) {
                puts("not an admin");
                char *msg = "You must be an admin to start a game\n";
                sendPromptToClient(clientSocket, msg);
                continue;
            }

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
            sendPromptToClient(clientSocket, playerList);
            // write(fd, playerList, strlen(playerList));

            continue;
        } else if (strcmp(data, "help") == 0) {
            sendPromptToClient(clientSocket, helpCommands);
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
        .playing = false,
    };


    game = (Game*)shm;
    game->users.users[0] = adminUser;
    game->users.nbUsers = 1;
    strcpy(game->session.name, "");

    // unsigned int osef = enbas[50][50] = {0};

    // game->session.grid = (unsigned[] int**) calloc(sizeof(unsigned int*),50);
    // if (game->session.grid == NULL) {
    //     perror("malloc");
    //     exit(1);
    // }
    // for (int i = 0; i < 50; i++)
    // {
    //     game->session.grid[i] = (unsigned int*) calloc(sizeof(unsigned int), 50);
    // }

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
    do
    {
        int sz = sizeof(struct sockaddr_in);
        int clientSocket = accept(sd, (struct sockaddr *) &client, (socklen_t*) &sz);
        if (clientSocket == -1) {
            perror("rip le socket");
            exit(1);
        }
        clientId++;
    
        int forked = fork();
        if (forked == 0) { //handle single client
            printf("[Child] New client with id %d\n", clientId);
            handleClient(clientSocket, clientId, fd);
            return EXIT_SUCCESS;
        }
        printf("[Main] New client with id %d\n", clientId);
    } while (1);
    
}
