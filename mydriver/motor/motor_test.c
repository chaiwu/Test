
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

static int fd;

/*
 * ./button_test /dev/100ask_button0
 *
 */
int main(int argc, char **argv)
{
	int buf;
	int ret;
	
	/* 1. 判断参数 */
	if (argc != 3) 
	{
		printf("Usage: %s <dev> <step_num> \n", argv[0]);
		return -1;
	}


	/* 2. 打开文件 */
	fd = open(argv[1], O_RDWR );
	if (fd == -1)
	{
		printf("can not open file %s\n", argv[1]);
		return -1;
	}

	buf = strtol(argv[2],NULL,0);

	ret = write(fd,&buf,4);
	if(ret != 2)
	{
		printf("write error\n");
	}
	
	close(fd);
	
	return 0;
}


