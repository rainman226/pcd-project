#include <stdio.h>          // for printf(), perror()
#include <stdlib.h>         // for exit(), malloc(), free()
#include <string.h>         // for memset(), strlen(), strchr(), strrchr()
#include <unistd.h>         // for close(), read(), write()
#include <arpa/inet.h>      // for sockaddr_in, AF_INET, SOCK_STREAM, INADDR_ANY, htons(), bind(), listen(), accept()
#include <sys/socket.h>     // for socket(), connect(), send(), recv()
#include <pthread.h>        // for pthread_create(), pthread_join()
#include <minizip/zip.h>    // for zipFile, zipOpen(), zipOpenNewFileInZip3(), zipWriteInFileInZip(), zipClose(), ZIP_OK
#include <dirent.h>         // for opendir(), readdir(), closedir()
#include <sys/stat.h>       // for stat(), struct stat
#include <time.h>           // for struct tm, localtime()

#define PORT 8080               // port number
#define ADMIN_PORT 8081         // admin port number
#define BUFFER_SIZE 4096        // buffer size
#define MAX_COMPRESSIONS 100

// struct to store information about a compression
typedef struct {
    int active;
    char source_dir[BUFFER_SIZE];
    char destination[BUFFER_SIZE];
    int compression_level;
    char password[BUFFER_SIZE];
    size_t total_size;
    size_t total_read;
} compression_info_t;

compression_info_t compressions[MAX_COMPRESSIONS];  // array to store compression information               
pthread_mutex_t compressions_mutex = PTHREAD_MUTEX_INITIALIZER;     // mutex to protect the compressions array

void *handle_client(void *client_socket);
void *handle_admin_client(void *client_socket);
void *client_listener(void *arg);
void *admin_listener(void *arg);
int add_file_to_zip(zipFile zf, const char *filepath, const char *password, int compression_level, compression_info_t *info);
int create_zip(const char *source_dir, const char *zip_path, const char *password, int compression_level, compression_info_t *info);
uLong tm_to_dosdate(const struct tm *ptm);
void print_progress(size_t current, size_t total);

int main() {
    int server_fd, admin_fd;
    pthread_t client_thread, admin_thread;

    memset(compressions, 0, sizeof(compressions));              // Initialize compressions array        

    // creating socket file descriptor for clients
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Client socket failed");
        exit(EXIT_FAILURE);
    }

    // creating socket file descriptor for admin clients
    if ((admin_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Admin socket failed");
        exit(EXIT_FAILURE);
    }

    // bind the client socket to the network
    struct sockaddr_in client_address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    if (bind(server_fd, (struct sockaddr *)&client_address, sizeof(client_address)) < 0) {
        perror("Client bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Bind the admin socket to the network
    struct sockaddr_in admin_address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(ADMIN_PORT)
    };

    if (bind(admin_fd, (struct sockaddr *)&admin_address, sizeof(admin_address)) < 0) {
        perror("Admin bind failed");
        close(admin_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming client connections
    if (listen(server_fd, 3) < 0) {
        perror("Client listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming admin connections
    if (listen(admin_fd, 3) < 0) {
        perror("Admin listen failed");
        close(admin_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d for clients and port %d for admins\n", PORT, ADMIN_PORT);

    // Create threads to handle client and admin connections
    pthread_create(&client_thread, NULL, client_listener, &server_fd);
    pthread_create(&admin_thread, NULL, admin_listener, &admin_fd);

    // Wait for the threads to finish (they won't in this case)
    pthread_join(client_thread, NULL);
    pthread_join(admin_thread, NULL);

    close(server_fd);
    close(admin_fd);
    return 0;
}

// Function to handle client connections
void *client_listener(void *arg) {
    int server_fd = *(int *)arg;
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t thread_id;

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Client accept failed");
            continue;
        }

        printf("Accepted new client connection\n");

        // Create a new thread for each client
        if (pthread_create(&thread_id, NULL, handle_client, (void *)&new_socket) != 0) {
            perror("Failed to create client thread");
            close(new_socket);
        }
    }
}


// Function to handle admin connections
void *admin_listener(void *arg) {
    int admin_fd = *(int *)arg;
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t thread_id;

    while (1) {
        if ((new_socket = accept(admin_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {       // accept connection from client
            perror("Admin accept failed");
            continue;
        }

        printf("Accepted new admin connection\n");

        // Create a new thread for each admin client
        if (pthread_create(&thread_id, NULL, handle_admin_client, (void *)&new_socket) != 0) {
            perror("Failed to create admin thread");
            close(new_socket);
        }
    }
}

// Function to handle client connections
void *handle_client(void *client_socket) {
    int sock = *(int *)client_socket;
    char buffer[BUFFER_SIZE] = {0};
    char source_dir[BUFFER_SIZE];
    char destination[BUFFER_SIZE];
    int compression_level;
    char password[BUFFER_SIZE];
    int i;

    // Read the source directory path, destination file path, compression level, and password from client
    read(sock, buffer, BUFFER_SIZE);
    sscanf(buffer, "%s %s %d %[^\n]", source_dir, destination, &compression_level, password);

    // we delete anything unwanted
    password[strcspn(password, "\n")] = '\0';

    printf("Password: '%s'\n", password); // verificare

    printf("Compressing directory: %s\n", source_dir);

    pthread_mutex_lock(&compressions_mutex);            // Lock the mutex to protect the compressions array

    // Find an empty slot in the compressions array
    for (i = 0; i < MAX_COMPRESSIONS; i++) {
        if (!compressions[i].active) {
            compressions[i].active = 1;
            strncpy(compressions[i].source_dir, source_dir, BUFFER_SIZE);
            strncpy(compressions[i].destination, destination, BUFFER_SIZE);
            compressions[i].compression_level = compression_level;
            strncpy(compressions[i].password, password, BUFFER_SIZE);
            compressions[i].total_size = 0;
            compressions[i].total_read = 0;
            break;
        }
    }
    pthread_mutex_unlock(&compressions_mutex);      // Unlock the mutex

    // check if there are too many compressions
    if (i == MAX_COMPRESSIONS) {
        send(sock, "Server is busy, try again later", strlen("Server is busy, try again later"), 0);
        close(sock);
        pthread_exit(NULL);
    }

    // create a ZIP archive of the directory
    if (create_zip(source_dir, destination, password, compression_level, &compressions[i]) != 0) {
        send(sock, "Directory compression failed", strlen("Directory compression failed"), 0);
    } else {
        send(sock, "Directory compressed successfully", strlen("Directory compressed successfully"), 0);
    }

    pthread_mutex_lock(&compressions_mutex);
    compressions[i].active = 0;
    pthread_mutex_unlock(&compressions_mutex);

    close(sock);
    pthread_exit(NULL);
}

// function to handle admin connections
void *handle_admin_client(void *client_socket) {
    int sock = *(int *)client_socket;
    char buffer[BUFFER_SIZE] = {0};

    printf("Handling admin client\n");

    pthread_mutex_lock(&compressions_mutex);                    // Lock the mutex to protect the compressions array

    // Send information about each active compression to the admin client
    for (int i = 0; i < MAX_COMPRESSIONS; i++) {
        if (compressions[i].active) {
            snprintf(buffer, BUFFER_SIZE, "Source: %s, Destination: %s, Level: %d, Progress: %zu/%zu\n",
                     compressions[i].source_dir, compressions[i].destination, compressions[i].compression_level,
                     compressions[i].total_read, compressions[i].total_size);
            printf("Sending: %s", buffer);
            send(sock, buffer, strlen(buffer), 0);
            memset(buffer, 0, BUFFER_SIZE);  // Clear buffer after sending
        }
    }
    pthread_mutex_unlock(&compressions_mutex);

    close(sock);
    printf("Admin client handled and closed\n");
    pthread_exit(NULL);
}

// Function to create a ZIP archive of a directory
int create_zip(const char *source_dir, const char *zip_path, const char *password, int compression_level, compression_info_t *info) {
    zipFile zf = zipOpen(zip_path, APPEND_STATUS_CREATE);   // Open a ZIP archive for writing 
    
    // check if the ZIP archive was opened successfully
    if (zf == NULL) {
        perror("zipOpen");
        return -1;
    }

    DIR *dir;
    struct dirent *entry;
    char filepath[BUFFER_SIZE];

    // open the source directory
    if ((dir = opendir(source_dir)) == NULL) {
        perror("opendir");
        return -1;
    }

    // iterate over each file in the directory
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {  // only process regular files
            snprintf(filepath, BUFFER_SIZE, "%s/%s", source_dir, entry->d_name);
            printf("Adding file: %s with password: '%s'\n", filepath, password);

            // add the file to the ZIP archive
            if (add_file_to_zip(zf, filepath, password, compression_level, info) != 0) {
                closedir(dir);
                zipClose(zf, NULL);
                return -1;
            }
        }
    }

    closedir(dir);
    zipClose(zf, NULL);
    return 0;
}

// Function to add a file to a ZIP archive
int add_file_to_zip(zipFile zf, const char *filepath, const char *password, int compression_level, compression_info_t *info) {
    FILE *file = fopen(filepath, "rb");     // open the file for reading
    if (!file) {
        perror("fopen");
        return -1;
    }

    zip_fileinfo zi;            // ZIP file information
    memset(&zi, 0, sizeof(zi)); // clear the ZIP file information

    // Get file modification time
    struct stat st;
    if (stat(filepath, &st) == 0) {
        struct tm *filedate = localtime(&st.st_mtime);
        zi.dosDate = tm_to_dosdate(filedate);
    }

    const char *filename_in_zip = strrchr(filepath, '/');   // get the filename from the path
    if (filename_in_zip == NULL) {
        filename_in_zip = filepath;
    } else {
        filename_in_zip++;
    }

    int err = zipOpenNewFileInZip3(                                 // open a new file in the ZIP archive
        zf, filename_in_zip, &zi, NULL, 0, NULL, 0, NULL,
        Z_DEFLATED, compression_level, 0,
        -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
        (password && strlen(password) > 0) ? password : NULL, 0);
    // check if the file was opened successfully
    if (err != ZIP_OK) {
        fclose(file);
        fprintf(stderr, "zipOpenNewFileInZip3 error: %d\n", err);
        return -1;
    }

    // read the file in chunks and write it to the ZIP archive
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    size_t total_read = 0;
    size_t total_size = st.st_size;

    pthread_mutex_lock(&compressions_mutex);
    info->total_size += total_size;
    pthread_mutex_unlock(&compressions_mutex);

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {        // read the file in chunks
        err = zipWriteInFileInZip(zf, buffer, bytes_read);                  // write the file to the ZIP archive
        if (err < 0) {
            fclose(file);
            fprintf(stderr, "zipWriteInFileInZip error: %d\n", err);
            return -1;
        }
        total_read += bytes_read;       // update the total read size

        pthread_mutex_lock(&compressions_mutex);
        info->total_read += bytes_read;
        pthread_mutex_unlock(&compressions_mutex);

        print_progress(info->total_read, info->total_size);
    }

    fclose(file);
    zipCloseFileInZip(zf);
    return 0;
}

// function to print a progress bar
void print_progress(size_t current, size_t total) {
    int bar_width = 70;
    float progress = (float)current / total;
    int pos = bar_width * progress;

    printf("[");
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) printf("=");
        else if (i == pos) printf(">");
        else printf(" ");
    }
    printf("] %d%%\r", (int)(progress * 100.0));
    fflush(stdout);
}

// function to convert a tm struct to a DOS date
uLong tm_to_dosdate(const struct tm *ptm) {
    uLong year = (uLong)ptm->tm_year + 1900;
    if (year < 1980) {
        year = 1980;
    } else if (year > 2107) {
        year = 2107;
    }
    return (uLong)(((year - 1980) << 25) | ((ptm->tm_mon + 1) << 21) | (ptm->tm_mday << 16) |
                   (ptm->tm_hour << 11) | (ptm->tm_min << 5) | (ptm->tm_sec >> 1));
}
