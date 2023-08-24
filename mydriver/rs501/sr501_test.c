
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <signal.h>

static int fd;

static void sig_func(int sig)
{
	int val;
	read(fd,&val,4);
	printf("get value %d\n",val);
}


int main(int argc, char **argv)
{
	int val;
	struct pollfd fds[1];
	int timeout_ms = 5000;
	int ret;
	int	flags;
	int i;
	
	/* 1. 判断参数 */
	if (argc != 2) 
	{
		printf("Usage: %s <dev>\n", argv[0]);
		return -1;
	}
	/* fasync signal */
	signal(SIGIO,sig_func);

	/* 2. 打开文件 */
	fd = open(argv[1], O_RDWR | O_NONBLOCK);
	if (fd == -1)
	{
		printf("can not open file %s\n", argv[1]);
		return -1;
	}
	/* fasync */
	fcntl(fd,F_SETOWN,getpid());
	flags = fcntl(fd,F_GETFL);
	fcntl(fd,F_SETFL, flags | FASYNC);


	/* poll */
	// fds[0].fd = fd;
	// fds[0].events = POLLIN;

	while (1)
	{
		// if (read(fd, &val, 4) == 4)
		// 	printf("get value %d\n", val);
		/* poll test */
		// ret = poll(fds, 1, timeout_ms);
		// if ((ret == 1) && (fds[0].revents & POLLIN))
		// {
		// 	read(fd, &val, 4);
		// 	printf("get value : %d\n", val);
		// }
		// else
		// {
		// 	printf("timeout\n");
		// }
		printf("test fasync\n");
		sleep(2);
	}
	
	close(fd);
	
	return 0;
}


