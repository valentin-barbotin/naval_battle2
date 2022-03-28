#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>


#define PORT 9871
#define HOST "127.0.0.1"

typedef char string[255];

unsigned int **grid;
char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

void receivePrompt(string message, int sd) {
    ssize_t resSize = 0;
    uint32_t sizeToReceive = 0;
    if (resSize = recv(sd, &sizeToReceive, sizeof(uint32_t), 0), resSize == -1) {
        perror("bye");
        exit(1);
    };
    sizeToReceive = ntohl(sizeToReceive);

    if (sizeToReceive > 0) {
        memset(message, 0, 255);
        if (resSize = recv(sd, message, sizeToReceive, 0), resSize == -1) {
            perror("bye");
            exit(1);
        };
        message[sizeToReceive] = '\0';
        puts(message);
    }
}

int main(int argc, int *argv[]) {
    struct sockaddr_in cl;
    int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    cl.sin_port = htons(PORT);
    cl.sin_addr.s_addr = inet_addr(HOST);
    cl.sin_family = AF_INET;

    if (connect(sd, (struct sockaddr *) &cl, sizeof(struct sockaddr_in)) == -1) {
        perror("Can't connect to server");
        return EXIT_FAILURE;
    };

    puts("connecté");
    char data[500] = {0};
    ssize_t resSize = 0;
    uint32_t sizeToReceive = 0;

    string msg = "";
    do
    {
        puts("Wait server response..");
        if (resSize = recv(sd, &sizeToReceive, sizeof(uint32_t), 0), resSize == -1) {
            perror("bye");
            exit(1);
        };
        // printf("resSize: %d\n", resSize);
        sizeToReceive = ntohl(sizeToReceive);
        printf("sizeToReceive: %u\n", sizeToReceive);
        if (sizeToReceive > 0) {
            puts("Must receive data, waiting..");
            memset(data, 0, 0);
            if (resSize = recv(sd, data, sizeToReceive, 0), resSize == -1) {
                perror("bye");
                exit(1);
            };
            data[sizeToReceive] = '\0';
            // printf("resSize: %d\n", resSize);
            puts(data);
        }

        receivePrompt(msg, sd);

        //check if there something to print
        puts("go saisi : ");
        fgets(data, 500, stdin);
        data[strcspn(data, "\n")] = 0;
        send(sd, data, 500, 0);
        puts("Message sended");
        // handle errors
    } while (1);
}
