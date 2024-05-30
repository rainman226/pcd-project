#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <zlib.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void *handle_client(void *client_socket);
int compress_file(const char *source, const char *destination, int level);

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t thread_id;

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the network
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        printf("Accepted new connection\n");

        // Create a new thread for each client
        if (pthread_create(&thread_id, NULL, handle_client, (void *)&new_socket) != 0) {
            perror("Failed to create thread");
            close(new_socket);
        }
    }

    close(server_fd);
    return 0;
}

void *handle_client(void *client_socket) {
    int sock = *(int *)client_socket;
    char buffer[BUFFER_SIZE] = {0};
    char source[BUFFER_SIZE];
    char destination[BUFFER_SIZE];
    int compression_level;

    // Read the source file path, destination file path, and compression level from client
    read(sock, buffer, BUFFER_SIZE);
    sscanf(buffer, "%s %s %d", source, destination, &compression_level);

    printf("Compressing file: %s\n", source);

    // Perform the file compression
    if (compress_file(source, destination, compression_level) == 0) {
        send(sock, "File compressed successfully", strlen("File compressed successfully"), 0);
    } else {
        send(sock, "File compression failed", strlen("File compression failed"), 0);
    }

    close(sock);
    pthread_exit(NULL);
}

int compress_file(const char *source, const char *destination, int level) {
    FILE *src = fopen(source, "rb");
    if (!src) {
        perror("Source file open error");
        return -1;
    }

    gzFile dst = gzopen(destination, "wb");
    if (!dst) {
        perror("Destination file open error");
        fclose(src);
        return -1;
    }

    if (gzsetparams(dst, level, Z_DEFAULT_STRATEGY) != Z_OK) {
        perror("Failed to set gzip parameters");
        fclose(src);
        gzclose(dst);
        return -1;
    }

    unsigned char buffer[BUFFER_SIZE];
    int num_read = 0;
    while ((num_read = fread(buffer, 1, BUFFER_SIZE, src)) > 0) {
        if (gzwrite(dst, buffer, num_read) != num_read) {
            perror("Failed to write to gzip file");
            fclose(src);
            gzclose(dst);
            return -1;
        }
    }

    fclose(src);
    gzclose(dst);

    return 0;
}
