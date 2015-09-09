
## Introduction

This is a small server and client application for prototyping the
new [memfd_create(2)](http://man7.org/linux/man-pages/man2/memfd_create.2.html)
Linux system call.

Memfd is a simple memory sharing mechanism, added by the systemd/kdbus
developers, to share pages between processes in an anonymous, no global
registry needed, no mount-point required, relatively secure, manner.
Check the references at the bottom of this page for primary documentation.

### Internal details

Server creates a classic Unix domain socket, and waits for clients to
connect.

Upon a client connection, server creates a memfd region and fill it with
a unique message. The file descriptor for this memory region is then
*sealed* and passed to the client using Unix domain sockets file-descriptor
passing mechanisms.

On the client side, upon connecting with the server, it recreives the
passed memfd file descriptor. Afterwards, the client tries to
break the `SHRINK`, `WRITE`, and `SEAL` memfd seals added by the server.

If everything goes as planned, the client can go and read the server-sent
message by `mmap()`-ing the passed file descriptor. This form of
communication is both zero-copy, and hopefully secure-enough, for zero-trust
IPC applications.

### Requirements

- Linux Kernel 3.17 or higher
- Header files for such a kernel
  - Debian/Ubuntu: `sudo apt-get install linux-headers-$(uname -r)`
  - Redhat/Fedora: `sudo yum -y kernel-headers-$(uname -r)`
  - Arch Linux: `sudo pacman -S linux-headers`

### References:
-  [On memfd_create(2)](https://dvdhrm.wordpress.com/2014/06/10/memfd_create2/), David Herrman (*memfd author*)
- [memfd_create(2) manpage](http://man7.org/linux/man-pages/man2/memfd_create.2.html)
- [The Linux Programming Interface](http://www.man7.org/tlpi/), Section 61.13.3 "Passing File Descriptors"
- [FD Passing for DRI.Next](http://keithp.com/blogs/fd-passing/), Keith Packard (*Xorg maintainer*)
