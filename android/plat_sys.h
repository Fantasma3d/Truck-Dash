/* sockets, threads, arquivos e tempo (POSIX). No Android: declaracoes proprias. */
#ifndef PLAT_SYS_H
#define PLAT_SYS_H
#include "plat.h"
#ifdef __ANDROID__
typedef int socklen_t;
typedef long ssize_t;
typedef unsigned long pthread_t;
typedef struct { int v[10]; } __attribute__((aligned(8))) pthread_mutex_t;
struct sockaddr { unsigned short sa_family; char sa_data[14]; };
struct in_addr { uint32_t s_addr; };
struct sockaddr_in { unsigned short sin_family; uint16_t sin_port; struct in_addr sin_addr; unsigned char sin_zero[8]; };
struct timeval { long tv_sec; long tv_usec; };
struct timespec { long tv_sec; long tv_nsec; };
#define AF_INET 2
#define SOCK_DGRAM 2
#define IPPROTO_UDP 17
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SO_BROADCAST 6
#define SO_RCVTIMEO 20
#define CLOCK_MONOTONIC 1
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 0100
#define O_TRUNC 01000
int socket(int, int, int);
int bind(int, const struct sockaddr *, socklen_t);
int connect(int, const struct sockaddr *, socklen_t);
int getsockname(int, struct sockaddr *, socklen_t *);
int setsockopt(int, int, int, const void *, socklen_t);
ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
ssize_t recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *);
int close(int);
int ioctl(int, int, ...);
int listen(int, int);
int accept(int, struct sockaddr *, socklen_t *);
ssize_t recv(int, void *, size_t, int);
#define SOCK_STREAM 1
struct ifreq { char ifr_name[16]; struct sockaddr ifr_addr; };
struct ifconf { int ifc_len; char *ifc_buf; };
#define SIOCGIFCONF 0x8912
int open(const char *, int, ...);
ssize_t read(int, void *, size_t);
ssize_t write(int, const void *, size_t);
int usleep(unsigned long);
int clock_gettime(int, struct timespec *);
int pthread_create(pthread_t *, const void *, void *(*)(void *), void *);
int pthread_detach(pthread_t);
int pthread_mutex_init(pthread_mutex_t *, const void *);
int pthread_mutex_lock(pthread_mutex_t *);
int pthread_mutex_unlock(pthread_mutex_t *);
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <net/if.h>
#endif
static inline uint16_t td_htons(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }
static inline double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
#endif
