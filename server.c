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
#define SHM_SIZE 4096
#define SHM_PERM (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)

bool isClientAdmin() {
    return true;
}

void handleClient(int clientSocket, int client, unsigned int fd, char *shm) {
    char data[500] = {0};
    unsigned int length = 0;
    ssize_t resSize = 0;
    char* helpCommands = "Here is the list of commands:\n"
    "- add: add a new user (admin)\n"
    "- remove: remove a user (admin)\n"
    "- login: <user>\n"
    "- list: list all users\n"
    "- create: create a new game (admin)\n"
    "- quit: quit the server (admin)\n"
    "- help: list all commands\n";

    uint32_t dataLen = strlen(helpCommands);
    uint32_t hostToNetInt = htonl(dataLen);
    send(clientSocket, &hostToNetInt, sizeof(hostToNetInt), 0);
    send(clientSocket, helpCommands, dataLen, 0);
    char *pos;
    do
    {
        // write(fd, "wait user input\n", strlen("wait user input\n"));
        if (resSize = recv(clientSocket, data, 500, 0), resSize == -1) {
            perror("bye");
            exit(1);
        };
        puts(data);
        // write(fd, "check action\n", strlen("check action\n"));
        if (strcmp(data, "exit") == 0) {
            puts("fin du programme");
            exit(0);
        } else if ((pos = strstr(data, "login")), pos != NULL) {
        //     // check if the user is already logged
            char *user = strstr(pos, " ");
            if (user == NULL) {
                exit(1);
            }
            user++;
            char *connectionFailed = "Connection failed\n";
            char *connectionSuccess = "Connection success\n";
            puts("the user is: ");
            puts(user);
            if (strcmp(user, "Admin") == 0) {
                dataLen = strlen(connectionSuccess);
                hostToNetInt = htonl(dataLen);
                send(clientSocket, &hostToNetInt, sizeof(hostToNetInt), 0);
                send(clientSocket, connectionSuccess, dataLen, 0);
            } else {
                dataLen = strlen(connectionFailed);
                hostToNetInt = htonl(dataLen);
                send(clientSocket, &hostToNetInt, sizeof(hostToNetInt), 0);
                send(clientSocket, connectionFailed, dataLen, 0);
            }

            char osef[500] = {0};
            if (*shm == '\0') {
                strcpy(osef, "No user..\n");
            } else {
                sprintf(osef, "The previous user was %s\n", user);
            }
            write(fd, osef, strlen(osef));
            strcpy(shm, user);
            continue;
        } else if (strcmp(data, "startgame") == 0) {
            // check if user is admin with id using Shared Memory
            // new 
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

void* createSHM() {
    return mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
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
    }
    memcpy(shm, "", 1);
    // strcpy(shm, "Admin");

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
            handleClient(clientSocket, clientId, fd, shm);
            return EXIT_SUCCESS;
        }
        printf("[Main] New client with id %d\n", clientId);
    } while (1);
    
}
