/**
 * Define _GNU_SOURCE to avoid following error:
 * error: implicit declaration of function ‘mkstemp’ [-Werror=implicit-function-declaration]
 */
#define _GNU_SOURCE

// Corresponding header
#include "debugclnt.h"

// System headers
#include <errno.h>
#include <stdio.h>  // snprintf(..)
#include <stdlib.h> // mkstemp(..)
#include <string.h> // strlen(..)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> // struct sockaddr_un
#include <unistd.h> // unlink(..)

#include "onu_hal_trace.h"


bool dbg_handle_command(const char* server_path, const void* msg, size_t len) {
    bool rv = false;
    int sockfd = 0;
    char client_name[128];

    client_name[0] = 0;

    if(access(server_path, F_OK) != 0) {
        SAH_TRACE_ERROR("server %s does not exist", server_path);
        return false;
    }

    sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if(sockfd == -1) {
        SAH_TRACE_ERROR("failed to create socket: %s", strerror(errno));
        return false;
    }

    struct sockaddr_un client_addr, server_addr;

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sun_family = AF_LOCAL;
    snprintf(client_name, 128, "%s", "/tmp/dbg.clientsock.XXXXXX");
    const int tmp_fd = mkstemp(client_name);
    if(tmp_fd == -1) {
        SAH_TRACE_ERROR("failed to make temporary file: %s", strerror(errno));
        goto exit;
    }

    close(tmp_fd);
    unlink(client_name);

    strcpy(client_addr.sun_path, client_name);

    if(bind(sockfd, (const struct sockaddr*) &client_addr, sizeof(client_addr)) == -1) {
        SAH_TRACE_ERROR("failed to bind client address: %s", strerror(errno));
        goto exit;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_LOCAL;
    strcpy(server_addr.sun_path, server_path);

    if(sendto(sockfd, msg, len, 0,
              (const struct sockaddr*) &server_addr, sizeof(server_addr)) == -1) {
        SAH_TRACE_ERROR("failed to send msg: %s", strerror(errno));
        goto exit;
    }

    rv = true;

exit:
    if(strlen(client_name)) {
        unlink(client_name);
    }
    if(sockfd > 0) {
        close(sockfd);
    }

    return rv;
}
