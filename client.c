/*
 * Example memfd_create(2) client application.
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "memfd.h"

#define errorp(msg) {				\
    perror("[Error] " msg);			\
    exit(EXIT_FAILURE);				\
}

#define error(...) {				\
    fprintf(stderr, "[Error] ");		\
    fprintf(stderr, __VA_ARGS__);		\
    fprintf(stderr, "\n");			\
}

#define info(...) {				\
    fprintf(stdout, "[Info ] ");		\
    fprintf(stdout, __VA_ARGS__);		\
    fprintf(stdout, "\n");			\
}

#define quit(...) {				\
    error(__VA_ARGS__);				\
    exit(EXIT_FAILURE);				\
}

/* Receive file descriptor passed from the server over
 * the already-connected unix domain socket @conn. */
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
        errorp("recvmsg()");

    if (size != 1) {
        error("Expected a placeholder message data of length 1");
        error("Received a message of length %d instead", size);
        quit("Exiting!");
    }

    cmsgh = CMSG_FIRSTHDR(&msgh);
    if (!cmsgh)
        quit("Expected one recvmsg() header with a passed memfd fd. "
	     "Got zero headers!");

    if (cmsgh->cmsg_level != SOL_SOCKET)
        quit("invalid cmsg_level %d", cmsgh->cmsg_level);
    if (cmsgh->cmsg_type != SCM_RIGHTS)
        quit("invalid cmsg_type %d", cmsgh->cmsg_type);

    return *((int *) CMSG_DATA(cmsgh));
}

#define LOCAL_SOCKET_NAME       "./unix_socket"

static int connect_to_server_and_get_memfd_fd() {
    int conn, ret;
    struct sockaddr_un address;

    conn = socket(PF_UNIX, SOCK_STREAM, 0);
    if (conn == -1)
        errorp("socket()");

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, UNIX_PATH_MAX, LOCAL_SOCKET_NAME);

    ret = connect(conn, (struct sockaddr *)&address, sizeof(struct sockaddr_un));
    if (ret != 0)
        errorp("connect()");

    return receive_fd(conn);
}

int main(int argc, char **argv) {
    char *shm, *shm1, *shm2;
    const int shm_size = 1024;
    int ret, fd, seals;

    fd = connect_to_server_and_get_memfd_fd();
    if (fd == -1)
        quit("Received invalid memfd fd from server equaling -1");

    seals = fcntl(fd, F_GET_SEALS);
    if (! (seals & F_SEAL_SHRINK))
        quit("Got non-sealed memfd. Expected an F_SEAL_SHRINK one");
    if (! (seals & F_SEAL_WRITE))
        quit("Got non-sealed memfd. Expected an F_SEAL_WRITE one");
    if (! (seals & F_SEAL_SEAL))
        quit("Got non-sealed memfd. Expected an F_SEAL_SEAL one");

    ret = ftruncate(fd, 0);
    if (ret != -1) {
	error("Server memfd F_SEAL_SHRINK protection is not working.");
        error("We were able to shrink the SHM area behind server's back!");
        error("This can easily introduce SIGBUS faults in the server.");
        quit("Exiting!");
    }

    ret = fcntl(fd, F_ADD_SEALS, F_SEAL_GROW);
    if (ret != -EPERM) {
        error("Server memfd F_SEAL_SEAL protection is not working");
        error("We were able to add an extra seal (GROW) to the memfd!");
        quit("Exiting!");
    }

    /* MAP_SHARED should fail on write-sealed memfds */
    shm1 = mmap(NULL, shm_size, PROT_READ, MAP_SHARED, fd, 0);
    shm2 = mmap(NULL, shm_size, PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm1 != MAP_FAILED || shm2 != MAP_FAILED) {
        error("Server memfd F_SEAL_WRITE protection is not working");
        error("We were able to succesfully map SHM area as writeable!");
        quit("Exiting!");
    }

    shm = mmap(NULL, shm_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (shm == MAP_FAILED)
        errorp("mmap");

    printf("Message: %s\n", shm);

    return 0;
}
