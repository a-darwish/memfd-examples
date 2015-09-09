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
#include <errno.h>

/*
 * Define memfd fcntl sealing macros. While they are already
 * defined in the kernel header file <linux/fcntl.h>, that file as
 * a whole conflicts with the original glibc header <fnctl.h>.
 */

#ifndef F_LINUX_SPECIFIC_BASE
#define F_LINUX_SPECIFIC_BASE 1024
#endif

#ifndef F_ADD_SEALS
#define F_ADD_SEALS (F_LINUX_SPECIFIC_BASE + 9)
#define F_GET_SEALS (F_LINUX_SPECIFIC_BASE + 10)

#define F_SEAL_SEAL     0x0001  /* prevent further seals from being set */
#define F_SEAL_SHRINK   0x0002  /* prevent file from shrinking */
#define F_SEAL_GROW     0x0004  /* prevent file from growing */
#define F_SEAL_WRITE    0x0008  /* prevent writes */
#endif

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
    char *shm, *shm1, *shm2;
    const int shm_size = 1024;
    int ret, fd, seals;

    fd = connect_to_server_and_get_memfd_fd();
    if (fd == -1)
        quit("Received invalid memfd fd from server equaling -1");

    seals = fcntl(fd, F_GET_SEALS);
    if (! (seals & F_SEAL_SHRINK))
        quit("Got non-sealed memfd. Expected an F_SEAL_SHRINK one\n");
    if (! (seals & F_SEAL_WRITE))
        quit("Got non-sealed memfd. Expected an F_SEAL_WRITE one\n");
    if (! (seals & F_SEAL_SEAL))
        quit("Got non-sealed memfd. Expected an F_SEAL_SEAL one\n");

    ret = ftruncate(fd, 0);
    if (ret != -1) {
        fprintf(stderr, "Server memfd F_SEAL_SHRINK protection is not working.\n");
        fprintf(stderr, "We were able to shrink the SHM area behind server's back!\n");
        fprintf(stderr, "This can easily introduce SIGBUS faults in the server.\n");
        quit("Exiting!\n");
    }

    ret = fcntl(fd, F_ADD_SEALS, F_SEAL_GROW);
    if (ret != -EPERM) {
        fprintf(stderr, "Server memfd F_SEAL_SEAL protection is not working\n");
        fprintf(stderr, "We were able to add an extra seal (GROW) to the memfd!\n");
        quit("Exiting!\n");
    }

    /* MAP_SHARED should fail on write-sealed memfds */
    shm1 = mmap(NULL, shm_size, PROT_READ, MAP_SHARED, fd, 0);
    shm2 = mmap(NULL, shm_size, PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm1 != MAP_FAILED || shm2 != MAP_FAILED) {
        fprintf(stderr, "Server memfd F_SEAL_WRITE protection is not working\n");
        fprintf(stderr, "We were able to succesfully map SHM area as writeable!\n");
        quit("Exiting!\n");
    }

    shm = mmap(NULL, shm_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (shm == MAP_FAILED)
        error("mmap");

    printf("Message: %s\n", shm);

    return 0;
}
