#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

// Server configuration
#define PORT 8080
#define BUFFER_SIZE 1024
#define SIXTEEN 16
#define TWOFIFTYSIX 256
#define FIVETWELVE 512
#define ROOT_DIR "./www"

// HTTP response templates
extern const char HTTP_200[];
extern const char HTTP_404[];
extern const char HTTP_400[];
extern const char HTTP_501[];

// Function declarations

/**
 * Thread function to handle a single client connection.
 *
 * @param arg A pointer to the client socket file descriptor.
 * @return A NULL pointer (threads automatically exit after completion).
 */
void *handle_client(void *arg);

#endif    // HTTP_SERVER_H
