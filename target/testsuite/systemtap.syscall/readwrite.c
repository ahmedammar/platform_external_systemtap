/* COVERAGE: read write readv writev lseek llseek */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <sys/uio.h>
#include <sys/syscall.h>

#define STRING1 "red"
#define STRING2 "green"
#define STRING3 "blue"
int main()
{
  int fd;
  struct iovec v[3], x[3];
  loff_t res;
  char buf[64], buf1[32], buf2[32], buf3[32];

  v[0].iov_base = STRING1;
  v[0].iov_len = sizeof(STRING1);
  v[1].iov_base = STRING2;
  v[1].iov_len = sizeof(STRING2);
  v[2].iov_base = STRING3;
  v[2].iov_len = sizeof(STRING3);

  fd = open("foobar1",O_WRONLY|O_CREAT, 0666);
  //staptest// open ("foobar1", O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?, 0666) = NNNN

  write(fd,"Hello world", 11);
  //staptest// write (NNNN, "Hello world", 11) = 11

  write(fd,"Hello world abcdefghijklmnopqrstuvwxyz 01234567890123456789", 59);
  //staptest// write (NNNN, "Hello world abcdefghijklmnopqrstuvwxyz 012345"..., 59) = 59

  pwrite(fd,"Hello Again",11,12);
  //staptest// pwrite (NNNN, "Hello Again", 11, 12) = 11

  writev(fd, v, 3);
  //staptest// writev (NNNN, XXXX, 3) = 15

  lseek(fd, 0, SEEK_SET);
  //staptest// lseek (NNNN, 0, SEEK_SET) = 0

  lseek(fd, 1, SEEK_CUR);
  //staptest// lseek (NNNN, 1, SEEK_CUR) = 1

  lseek(fd, -1, SEEK_END);
  //staptest// lseek (NNNN, -1, SEEK_END) = 84

#ifdef SYS__llseek
  syscall(SYS__llseek, fd, 1, 0, &res, SEEK_SET);
  //staptest// llseek (NNNN, 0x1, 0x0, XXXX, SEEK_SET) = 0

  syscall(SYS__llseek, fd, 0, 0, &res, SEEK_SET);
  //staptest// llseek (NNNN, 0x0, 0x0, XXXX, SEEK_SET) = 0

  syscall(SYS__llseek, fd, 0, 12, &res, SEEK_CUR);
  //staptest// llseek (NNNN, 0x0, 0xc, XXXX, SEEK_CUR) = 0

  syscall(SYS__llseek, fd, 8, 1, &res, SEEK_END);
  //staptest// llseek (NNNN, 0x8, 0x1, XXXX, SEEK_END) = 0
#endif

  close (fd);

  fd = open("foobar1",O_RDONLY);
  //staptest// open ("foobar1", O_RDONLY[[[[.O_LARGEFILE]]]]?) = NNNN

  read(fd, buf, 11);
  //staptest// read (NNNN, XXXX, 11) = 11

  read(fd, buf, 50);
  //staptest// read (NNNN, XXXX, 50) = 50

  pread(fd, buf, 11, 10);
  //staptest// pread (NNNN, XXXX, 11, 10) = 11

  x[0].iov_base = buf1;
  x[0].iov_len = sizeof(STRING1);
  x[1].iov_base = buf2;
  x[1].iov_len = sizeof(STRING2);
  x[2].iov_base = buf3;
  x[2].iov_len = sizeof(STRING3);
  readv(fd, x, 3);
  //staptest// readv (NNNN, XXXX, 3) = 15

  close (fd);

  return 0;
}
