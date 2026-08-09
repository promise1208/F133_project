#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/mman.h>

int main(int argc, char *argv[])
{
	int devfd, val, size = 4096;
	unsigned int ioctl_val = 0;
	char *src = "hello world\n";
	char dst[128];

	devfd = open("/dev/chrdev_dev", O_RDWR);
	if (devfd < 0) {
		printf("open /dev/chrdev fail\n");
		return -1;
	}
	
	write(devfd, src, strlen(src));
	read(devfd, dst, strlen(src));

	ioctl_val = 1;
	ioctl(devfd, 1111, &ioctl_val);
	ioctl_val = 3;
	ioctl(devfd, 2222, &ioctl_val);

	close(devfd);

	printf("dst  = %s\n", dst);

	return 0;
}


