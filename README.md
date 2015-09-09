
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

Upon a client connection, a memfd region is created and filled with a
unique message. The file descriptor for this memory region is then
passed to the client using unix sockets fd-passing mechanisms.

The client can then read the message from the server, through the
passed file descriptor, in a zero-copy and sealed manner.

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
