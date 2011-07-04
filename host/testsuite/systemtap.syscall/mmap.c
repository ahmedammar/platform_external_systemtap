/* COVERAGE: mmap2 munmap msync mlock mlockall munlock munlockall mprotect mremap fstat open close */
#include <sys/types.h>
#include <sys/stat.h>
#define __USE_GNU
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
	int fd, ret;
	struct stat fs;
	void * r;

	/* create a file with something in it */
	fd = open("foobar",O_WRONLY|O_CREAT|O_TRUNC, 0600);
	//staptest// open ("foobar", O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?|O_TRUNC, 0600) = NNNN
	lseek(fd, 1024, SEEK_SET);
	write(fd, "abcdef", 6);
	close(fd);
	//staptest// close (NNNN) = 0

	fd = open("foobar", O_RDONLY);
	//staptest// open ("foobar", O_RDONLY[[[[.O_LARGEFILE]]]]?) = NNNN

	/* stat for file size */
	ret = fstat(fd, &fs);
	//staptest// fstat (NNNN, XXXX) = 0

	r = mmap(NULL, fs.st_size, PROT_READ, MAP_SHARED, fd, 0);
	//staptest// mmap[2]* (XXXX, 1030, PROT_READ, MAP_SHARED, NNNN, XXXX) = XXXX

	close(fd);

	mlock(r, fs.st_size);
	//staptest// mlock (XXXX, 1030) = 0

	msync(r, fs.st_size, MS_SYNC);	
	//staptest// msync (XXXX, 1030, MS_SYNC) = 0

	munlock(r, fs.st_size);
	//staptest// munlock (XXXX, 1030) = 0

	mlockall(MCL_CURRENT);
	//staptest// mlockall (MCL_CURRENT) = 

	munlockall();
	//staptest// munlockall () = 0

	munmap(r, fs.st_size);
	//staptest// munmap (XXXX, 1030) = 0

	r = mmap(NULL, 12288, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	//staptest// mmap[2]* (XXXX, 12288, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = XXXX

	mprotect(r, 4096, PROT_READ);
	//staptest// mprotect (XXXX, 4096, PROT_READ) = 0

	munmap(r, 12288);
	//staptest// munmap (XXXX, 12288) = 0

	r = mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	//staptest// mmap[2]* (XXXX, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = XXXX

	r = mremap(r, 8192, 4096, 0);
	//// mremap (XXXX, 8192, 4096, 0) = XXXX

	munmap(r, 4096);
	//// munmap (XXXX, 4096) = 0

	return 0;
}
