#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void send_directory(int sock, const char *source_dir, const char *dest_file, int compression_level);

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char source_dir[BUFFER_SIZE];
    char destination[BUFFER_SIZE];
    char dest_file[BUFFER_SIZE];
    int compression_level;

    // Get directory path and compression level from the user
    printf("Enter source directory path: ");
    scanf("%s", source_dir);
    printf("Enter destination file path (including filename): ");
    scanf("%s", destination);
    printf("Enter compression level (0-9): ");
    scanf("%d", &compression_level);

    // Construct the full destination file path
    snprintf(dest_file, BUFFER_SIZE, "%s.gz", destination);

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

    // Send directory path, destination file path, and compression level to the server
    send_directory(sock, source_dir, dest_file, compression_level);

    // Receive server response
    char buffer[BUFFER_SIZE] = {0};
    read(sock, buffer, BUFFER_SIZE);
    printf("Server: %s\n", buffer);

    close(sock);
    return 0;
}

void send_directory(int sock, const char *source_dir, const char *dest_file, int compression_level) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "%s %s %d", source_dir, dest_file, compression_level);
    send(sock, buffer, strlen(buffer), 0);
}
