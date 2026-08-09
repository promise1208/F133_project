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
	char *str = "hello world\n";
	char buf[4096];
	int devfd;

	devfd = open("/dev/my_miscdevice", O_RDWR);
	if (devfd < 0) {
		printf("open /dev/my_miscdevice fail\n");
		return -1;
	}
	
	write(devfd, str, strlen(str));
	read(devfd, buf, strlen(str));

	close(devfd);

	printf("buf = %s\n", buf);

	return 0;
}


