#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <zlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libtar.h>

#define PORT 8080
#define BUFFER_SIZE 4096  // Increased buffer size to avoid truncation warnings

void *handle_client(void *client_socket);
int create_tar(const char *source_dir, const char *tar_path);
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
    char source_dir[BUFFER_SIZE];
    char destination[BUFFER_SIZE];
    int compression_level;

    // Read the source directory path, destination file path, and compression level from client
    read(sock, buffer, BUFFER_SIZE);
    sscanf(buffer, "%s %s %d", source_dir, destination, &compression_level);

    printf("Compressing directory: %s\n", source_dir);

    // Create a tarball of the directory
    char tar_path[BUFFER_SIZE];
    snprintf(tar_path, BUFFER_SIZE, "%s.tar", destination);
    if (create_tar(source_dir, tar_path) != 0) {
        send(sock, "Directory compression failed", strlen("Directory compression failed"), 0);
        close(sock);
        pthread_exit(NULL);
    }

    // Compress the tarball with gzip
    if (compress_file(tar_path, destination, compression_level) != 0) {
        send(sock, "Directory compression failed", strlen("Directory compression failed"), 0);
    } else {
        send(sock, "Directory compressed successfully", strlen("Directory compressed successfully"), 0);
    }

    // Remove the tarball after compression
    remove(tar_path);

    close(sock);
    pthread_exit(NULL);
}

int create_tar(const char *source_dir, const char *tar_path) {
    TAR *pTar;
    if (tar_open(&pTar, tar_path, NULL, O_WRONLY | O_CREAT, 0644, TAR_GNU) == -1) {
        perror("tar_open");
        return -1;
    }

    DIR *dir;
    struct dirent *entry;
    char filepath[BUFFER_SIZE];

    if ((dir = opendir(source_dir)) == NULL) {
        perror("opendir");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {  // Only process regular files
            snprintf(filepath, BUFFER_SIZE, "%s/%s", source_dir, entry->d_name);
            if (tar_append_file(pTar, filepath, entry->d_name) != 0) {
                perror("tar_append_file");
                closedir(dir);
                tar_close(pTar);
                return -1;
            }
        }
    }

    closedir(dir);

    if (tar_append_eof(pTar) != 0) {
        perror("tar_append_eof");
        tar_close(pTar);
        return -1;
    }

    if (tar_close(pTar) != 0) {
        perror("tar_close");
        return -1;
    }

    return 0;
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
    int num_read;
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
