/*
 * Example memfd_create(2) client application.
 *
 * Copyright (C) 2015 Ahmed S. Darwish <darwish.07@gmail.com>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, version 2 or later.
 *
 * Kindly check attached README.md file for further details.
 */

#include <sys/syscall.h>
#include <linux/memfd.h>
#include <sys/mman.h>

#include <sys/socket.h>
#include <linux/un.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static void error(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void quit(const char* fmt, ...) {
  va_list arglist;

  va_start(arglist, fmt);
  vfprintf(stderr, fmt, arglist );
  va_end( arglist );
  exit(EXIT_FAILURE);
}

/* Receive file descriptor sent from the server over
 * the already-connected socket @conn. */
static int receive_fd(int conn) {
    struct msghdr msgh;
    struct iovec iov;
    union {
        struct cmsghdr cmsgh;
        /* Space large enough to hold an 'int' */
        char   control[CMSG_SPACE(sizeof(int))];
    } control_un;
    struct cmsghdr *cmsgh;

    /* The sender must transmit at least 1 byte of real data
     * in order to send some other ancillary data (the fd). */
    char placeholder;
    iov.iov_base = &placeholder;
    iov.iov_len = sizeof(char);

    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof(control_un.control);

    int size = recvmsg(conn, &msgh, 0);
    if (size == -1)
        error("recvmsg()");

    if (size != 1) {
        fprintf(stderr, "Expected a placeholder message data of length 1\n");
        fprintf(stderr, "Received a message of length %d instead\n", size);
        quit("Exiting!\n");
    }

    cmsgh = CMSG_FIRSTHDR(&msgh);
    if (cmsgh->cmsg_level != SOL_SOCKET)
        quit("invalid cmsg_level %d\n", cmsgh->cmsg_level);
    if (cmsgh->cmsg_type != SCM_RIGHTS)
        quit("invalid cmsg_type %d\n", cmsgh->cmsg_type);

    return *((int *) CMSG_DATA(cmsgh));
}

#define LOCAL_SOCKET_NAME       "./unix_socket"

static int connect_to_server_and_get_memfd_fd() {
    int conn, ret;
    struct sockaddr_un address;

    conn = socket(PF_UNIX, SOCK_STREAM, 0);
    if (conn == -1)
        error("socket()");

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, UNIX_PATH_MAX, LOCAL_SOCKET_NAME);

    ret = connect(conn, (struct sockaddr *)&address, sizeof(struct sockaddr_un));
    if (ret != 0)
        error("connect()");

    return receive_fd(conn);
}

int main(int argc, char **argv) {
    char *shm;
    const int shm_size = 1024;
    int ret, fd;

    fd = connect_to_server_and_get_memfd_fd();
    if (fd == -1)
        quit("Received invalid memfd fd from server equaling -1");

    ret = ftruncate(fd, 0);
    if (ret != -1) {
        fprintf(stderr, "Server memfd F_SEAL_SHRINK protection is not working\n");
        fprintf(stderr, "We were able to shrink the SHM area behind server's back\n");
        fprintf(stderr, "This can easily introduce SIGBUS faults in the server\n");
        quit("Exiting!\n");
    }

    shm = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NORESERVE, fd, 0);
    if (shm == MAP_FAILED)
        error("mmap");

    printf("Message: %s\n", shm);

    return 0;
}
