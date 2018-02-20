/*
 * Example memfd_create(2) server application.
 *
 * Copyright (C) 2015 Ahmed S. Darwish <darwish.07@gmail.com>
 *
 * SPDX-License-Identifier: Unlicense
 *
 * Kindly check attached README.md file for further details.
 */

#include <sys/syscall.h>
#include <linux/memfd.h>
#include <sys/mman.h>

#include <sys/socket.h>
#include <linux/un.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>

#include "memfd.h"

static void error(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static int new_memfd_region(char *unique_str) {
    char *shm;
    const int shm_size = 1024;
    int fd, ret;

    fd = memfd_create("Server memfd", MFD_ALLOW_SEALING);
    if (fd == -1)
        error("memfd_create()");

    ret = ftruncate(fd, shm_size);
    if (ret == -1)
        error("ftruncate()");

    ret = fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK);
    if (ret == -1)
        error("fcntl(F_SEAL_SHRINK)");

    shm = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED)
        error("mmap()");

    sprintf(shm, "Secure zero-copy message from server: %s", unique_str);

    /* Seal writes too, but unmap our shared mappings beforehand */
    ret = munmap(shm, shm_size);
    if (ret == -1)
        error("munmap()");
    ret = fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE);
    if (ret == -1)
        error("fcntl(F_SEAL_WRITE)");

    ret = fcntl(fd, F_ADD_SEALS, F_SEAL_SEAL);
    if (ret == -1)
        error("fcntl(F_SEAL_SEAL)");

    return fd;
}

/* Pass file descriptor @fd to the client, which is
 * connected to us through the Unix domain socket @conn */
static void send_fd(int conn, int fd) {
    struct msghdr msgh;
    struct iovec iov;
    union {
        struct cmsghdr cmsgh;
        /* Space large enough to hold an 'int' */
        char   control[CMSG_SPACE(sizeof(int))];
    } control_un;

    if (fd == -1) {
        fprintf(stderr, "Cannot pass an invalid fd equaling -1\n");
        exit(EXIT_FAILURE);
    }

    /* We must transmit at least 1 byte of real data in order
     * to send some other ancillary data. */
    char placeholder = 'A';
    iov.iov_base = &placeholder;
    iov.iov_len = sizeof(char);

    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof(control_un.control);

    /* Write the fd as ancillary data */
    control_un.cmsgh.cmsg_len = CMSG_LEN(sizeof(int));
    control_un.cmsgh.cmsg_level = SOL_SOCKET;
    control_un.cmsgh.cmsg_type = SCM_RIGHTS;
    *((int *) CMSG_DATA(CMSG_FIRSTHDR(&msgh))) = fd;

    int size = sendmsg(conn, &msgh, 0);
    if (size < 0)
        error("sendmsg()");
}

#define LOCAL_SOCKET_NAME    "./unix_socket"
#define MAX_CONNECT_BACKLOG  128
#define CTIME_BUFFER_LEN     30

static void start_server_and_send_memfd_to_clients() {
    int sock, conn, fd, ret;
    struct sockaddr_un address;
    socklen_t addrlen;


    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock == -1)
        error("socket()");

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, UNIX_PATH_MAX, LOCAL_SOCKET_NAME);

    ret = unlink(LOCAL_SOCKET_NAME);
    if (ret != 0 && ret != -ENOENT && ret != -EPERM)
        error("unlink()");

    ret = bind(sock, (struct sockaddr *) &address, sizeof(address));
    if (ret != 0)
        error("bind()");

    ret = listen(sock, MAX_CONNECT_BACKLOG);
    if (ret != 0)
        error("listen()");

    while (true) {
        conn = accept(sock, (struct sockaddr *) &address, &addrlen);
        if (conn == -1)
            break;

        /* Remove useless ctime(3) trailing newline */
        time_t now = time(NULL);
        char *nowbuf = ctime(&now);
        nowbuf[strlen(nowbuf) - 1] = '\0';

        printf("[%s] New client connection!\n", nowbuf);

        fd = new_memfd_region(nowbuf);
        send_fd(conn, fd);

        close(conn);
        close(fd);
    }
}

int main(int argc, char **argv) {
    start_server_and_send_memfd_to_clients();
    return 0;
}
