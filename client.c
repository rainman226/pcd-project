#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 4096

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char source[BUFFER_SIZE];
    char destination[BUFFER_SIZE];
    char password[BUFFER_SIZE];
    int compression_level;

    // Get input from the user
    printf("Enter source directory path: ");
    scanf("%s", source);
    printf("Enter destination file path (including filename, without extension): ");
    scanf("%s", destination);
    printf("Enter compression level (0-9): ");
    scanf("%d", &compression_level);
    printf("Enter password (or leave blank for no password): ");
    getchar(); // To consume the newline left by the previous scanf
    fgets(password, BUFFER_SIZE, stdin);
    password[strcspn(password, "\n")] = '\0'; // Remove the trailing newline

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    // Send the directory path, destination file path, compression level, and password to the server
    snprintf(buffer, BUFFER_SIZE, "%s %s %d %s", source, destination, compression_level, password);
    send(sock, buffer, strlen(buffer), 0);

    // Receive server response
    read(sock, buffer, BUFFER_SIZE);
    printf("Server: %s\n", buffer);

    close(sock);
    return 0;
}
