#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#define IOC_AT24C02_READ 100
#define IOC_AT24C02_WRITE 101

static int fd;



/*
 * ./at24c02_test /dev/liqing_at24c02 r 10
 * ./at24c02_test /dev/liqing_at24c02 w 10 a
 */
int main(int argc, char **argv)
{
	unsigned char buf[2];	
	/* 1. 判断参数 */
	if ((argc != 4) && (argc != 5)) 
	{
		printf("Usage: %s <dev> r <addr>\n", argv[0]);
		printf("Usage: %s <dev> w <addr> <val>\n", argv[0]);
		return -1;
	}


	/* 2. 打开文件 */
	fd = open(argv[1], O_RDWR );
	if (fd == -1)
	{
		printf("can not open file %s\n", argv[1]);
		return -1;
	}


	if(argv[2][0] == 'r')
	{
		buf[0] = argv[3][0];
		// printf("buf[] = %c\n",buf[0]);
		ioctl(fd,IOC_AT24C02_READ,buf);
		printf("read addr %d, get data %c\n",buf[0],buf[1]);
	}
	else
	{
		buf[0] = argv[3][0];
		buf[1] = argv[4][0];
		// printf("buf[1] = %c\n",buf[1]);
		ioctl(fd,IOC_AT24C02_WRITE,buf);
		// printf(buf[1]);

	}
	
	close(fd);
	
	return 0;
}


