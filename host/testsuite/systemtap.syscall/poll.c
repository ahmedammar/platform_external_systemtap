/* COVERAGE: epoll_create epoll_ctl epoll_wait poll ppoll */
#define _GNU_SOURCE
#include <sys/epoll.h>
#include <poll.h>
#include <signal.h>
#include <sys/syscall.h>

int main()
{
  struct epoll_event ev;
  struct pollfd pfd = {7, 0x23, 0};
  int fd;
  struct timespec tim = {.tv_sec=0, .tv_nsec=200000000};
  sigset_t sigs;

  sigemptyset(&sigs);
  sigaddset(&sigs,SIGUSR2);

#ifdef EPOLL_CLOEXEC
  fd = epoll_create1(EPOLL_CLOEXEC);
  //staptest// epoll_create1 (EPOLL_CLOEXEC)
#else
  fd = epoll_create(32);
  //staptest// epoll_create (32)
#endif

  epoll_ctl(fd, EPOLL_CTL_ADD, 13, &ev);
  //staptest// epoll_ctl (NNNN, EPOLL_CTL_ADD, 13, XXXX)

  epoll_wait(fd, &ev, 17,0);
  //staptest// epoll_wait (NNNN, XXXX, 17, 0)
  close(fd);

  poll(&pfd, 1, 0);
  //staptest// poll (XXXX, 1, 0)

#ifdef SYS_ppoll
  ppoll(&pfd, 1, &tim, &sigs);
  //staptest//  ppoll (XXXX, 1, \[0.200000000\], XXXX, 8)
#endif

  return 0;
}
