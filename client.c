#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char source[BUFFER_SIZE];
    char destination[BUFFER_SIZE];
    int compression_level;
    char dest_file[BUFFER_SIZE];

    // Get file paths and compression level from the user
    printf("Enter source file path: ");
    scanf("%s", source);
    printf("Enter destination directory: ");
    scanf("%s", destination);
    printf("Enter compression level (0-9): ");
    scanf("%d", &compression_level);

    // Extract filename from source path
    char *filename = strrchr(source, '/');
    if (filename == NULL) {
        filename = source;
    } else {
        filename++;  // Skip the '/'
    }

    // Construct the full destination file path
    snprintf(dest_file, BUFFER_SIZE, "%s/%s.gz", destination, filename);

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

    // Send file paths and compression level to the server
    snprintf(buffer, BUFFER_SIZE, "%s %s %d", source, dest_file, compression_level);
    send(sock, buffer, strlen(buffer), 0);

    // Receive server response
    read(sock, buffer, BUFFER_SIZE);
    printf("Server: %s\n", buffer);

    close(sock);
    return 0;
}
