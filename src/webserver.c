#include "webserver.h"

// HTTP response templates
const char HTTP_200[] = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n";
const char HTTP_404[] = "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n<html><body><h1>404 Not Found</h1></body></html>\r\n";
const char HTTP_400[] = "HTTP/1.0 400 Bad Request\r\nContent-Type: text/html\r\n\r\n<html><body><h1>400 Bad Request</h1></body></html>\r\n";
const char HTTP_501[] = "HTTP/1.0 501 Not Implemented\r\nContent-Type: text/html\r\n\r\n<html><body><h1>501 Not Implemented</h1></body></html>\r\n";

// Thread function to handle a single client connection
void *handle_client(void *arg)
{
    ssize_t     valread;
    struct stat file_stat;
    char        buffer[BUFFER_SIZE];      // Buffer for client request
    char        method[SIXTEEN];          // HTTP method (e.g., GET, HEAD)
    char        uri[TWOFIFTYSIX];         // Requested URI (e.g., /index.html)
    char        version[SIXTEEN];         // HTTP version (e.g., HTTP/1.0)
    char        file_path[FIVETWELVE];    // Full file path on server
    FILE       *file;

    int client_fd = *(int *)arg;    // Extract client socket file descriptor
    free(arg);
    // Initialize variables
    memset(buffer, 0, BUFFER_SIZE);
    memset(method, 0, SIXTEEN);
    memset(uri, 0, TWOFIFTYSIX);
    memset(version, 0, SIXTEEN);
    memset(file_path, 0, FIVETWELVE);

    // Read the HTTP request from the client
    valread = read(client_fd, buffer, BUFFER_SIZE - 1);
    if(valread <= 0)
    {
        perror("Error reading from client");
        close(client_fd);
        pthread_exit(NULL);
    }
    buffer[valread] = '\0';    // Null-terminate the buffer

    // Parse the HTTP request line (e.g., "GET /index.html HTTP/1.1")
    if(sscanf(buffer, "%15s %255s %15s", method, uri, version) != 3)
    {
        printf("Error: Bad Request (400) - Malformed request\n\n");
        write(client_fd, HTTP_400, strlen(HTTP_400));
        close(client_fd);
        pthread_exit(NULL);
    }

    // Log the parsed request details
    printf("Client Request: Method=%s, URI=%s, Version=%s\n", method, uri, version);

    // Validate the HTTP version
    if(strcmp(version, "HTTP/1.0") != 0 && strcmp(version, "HTTP/1.1") != 0)
    {
        printf("Error: Bad Request (400) - Unsupported HTTP version: %s\n\n", version);
        write(client_fd, HTTP_400, strlen(HTTP_400));
        close(client_fd);
        pthread_exit(NULL);
    }

    // Check for unsupported HTTP/2 upgrade requests
    if(strstr(buffer, "Upgrade:") && strstr(buffer, "h2c"))
    {
        printf("Error: Bad Request (400) - HTTP/2 Upgrade requested\n\n");
        write(client_fd, HTTP_400, strlen(HTTP_400));
        close(client_fd);
        pthread_exit(NULL);
    }

    // Validate the HTTP method
    if(strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0)
    {
        printf("Error: Not Implemented (501) - Unsupported method: %s\n\n", method);
        write(client_fd, HTTP_501, strlen(HTTP_501));
        close(client_fd);
        pthread_exit(NULL);
    }

    // Validate the URI
    if(uri[0] != '/')
    {
        printf("Error: Bad Request (400) - Invalid URI: %s\n\n", uri);
        write(client_fd, HTTP_400, strlen(HTTP_400));
        close(client_fd);
        pthread_exit(NULL);
    }

    // Construct the full file path
    snprintf(file_path, sizeof(file_path), "%s%s", ROOT_DIR, uri[1] ? uri : "/index.html");
    printf("Resolved file path: %s\n", file_path);

    // Check if the file exists and is not a directory
    if(stat(file_path, &file_stat) < 0 || S_ISDIR(file_stat.st_mode))
    {
        printf("Error: Not Found (404) - File: %s\n\n", file_path);
        write(client_fd, HTTP_404, strlen(HTTP_404));
        close(client_fd);
        pthread_exit(NULL);
    }

    // Attempt to open the file
    file = fopen(file_path, "re");
    if(!file)
    {
        perror("Error opening file");
        write(client_fd, HTTP_404, strlen(HTTP_404));
        close(client_fd);
        pthread_exit(NULL);
    }

    // Log successful response
    printf("Success: OK (200) - File served: %s\n\n", file_path);

    // Respond with HTTP 200 OK
    write(client_fd, HTTP_200, strlen(HTTP_200));

    // Send the file content if it's a GET request
    if(strcmp(method, "GET") == 0)
    {
        char   file_buffer[BUFFER_SIZE];
        size_t bytes_read;

        while((bytes_read = fread(file_buffer, 1, BUFFER_SIZE, file)) > 0)
        {
            write(client_fd, file_buffer, bytes_read);
        }
    }

    // Clean up
    fclose(file);
    close(client_fd);
    pthread_exit(NULL);
}

int main(void)
{
    int                server_fd;
    int                opt = 1;
    pthread_t          thread_id;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t          client_addrlen = sizeof(client_addr);

    // Create a socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0)
    {
        perror("Error creating socket");
        return EXIT_FAILURE;
    }

    // Allow address reuse
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("Error setting socket options");
        close(server_fd);
        return EXIT_FAILURE;
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error binding socket");
        close(server_fd);
        return EXIT_FAILURE;
    }

    // Start listening for connections
    if(listen(server_fd, SOMAXCONN) < 0)
    {
        perror("Error listening on socket");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("Server running on port %d\n", PORT);

    // Main server loop
    while(1)
    {
        int *client_fd = malloc(sizeof(int));
        if(!client_fd)
        {
            perror("Error allocating memory");
            continue;
        }

        // Accept a new client connection
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addrlen);
        if(*client_fd < 0)
        {
            perror("Error accepting connection");
            free(client_fd);
            continue;
        }

        // Create a new thread to handle the client
        if(pthread_create(&thread_id, NULL, handle_client, client_fd) != 0)
        {
            perror("Error creating thread");
            free(client_fd);
            continue;
        }

        // Detach the thread to free resources automatically
        pthread_detach(thread_id);
    }
}
