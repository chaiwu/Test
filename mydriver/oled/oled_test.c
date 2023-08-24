#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "font.h"
#include "KUNKUN.h"

#define IOC_OLED_CLEAR 100
#define IOC_OLED_WRITE 101
#define IOC_OLED_SETXY 102
#define IOC_OLED_KUNKUN 103

static int fd;

void OLED_DIsp_Char(int x, int y, unsigned char c)
{
	int i = 0;
	unsigned char buf[3];
	/* 得到字模 */
	const unsigned char *dots = oled_asc2_8x16[c - ' '];

	// OLED_DIsp_Set_Pos(x, y);
	buf[0] = (unsigned char)x;
	buf[1] = (unsigned char)y;
	ioctl(fd,IOC_OLED_SETXY,buf);
	/* 发出8字节数据 */
	for (i = 0; i < 8; i++)
	{
		buf[0] = dots[i];
		ioctl(fd,IOC_OLED_WRITE,buf);
	}
		// oled_write_cmd_data(dots[i], OLED_DATA);
	buf[0] = (unsigned char)x;
	buf[1] = (unsigned char)(y+1);
	ioctl(fd,IOC_OLED_SETXY,buf);
	/* 发出8字节数据 */
	for (i = 0; i < 8; i++)
	{
		buf[0] = dots[i+8];
		ioctl(fd,IOC_OLED_WRITE,buf);
	}

		// oled_write_cmd_data(dots[i+8], OLED_DATA);
}

void OLED_DIsp_String(int x, int y, char *str)
{
	unsigned char j=0;
	while (str[j])
	{		
		OLED_DIsp_Char(x, y, str[j]);//显示单个字符
		x += 8;
		if(x > 127)
		{
			x = 0;
			y += 2;
		}//移动显示位置
		j++;
	}
}

void show_kunkun()
{
	unsigned char x, y;
	unsigned char buf[2];

    for (y = 0; y < 8; y++)
    {
        // OLED_DIsp_Set_Pos(0, y);
		buf[0] = (unsigned char)x;
		buf[1] = (unsigned char)y;
		ioctl(fd,IOC_OLED_SETXY,buf);

        for (x = 0; x < 64; x++)
		{
			ioctl(fd,IOC_OLED_WRITE,name[0][y*64+x]);
		}
            // oled_write_cmd_data(0, OLED_DATA); /* 清零 */
    }

}
/*
 * ./at24c02_test /dev/liqing_at24c02 r 10
 * ./at24c02_test /dev/liqing_at24c02 w 10 a
 */
int main(int argc, char **argv)
{
	unsigned char buf[2];	
	int i;
	/* 1. 判断参数 */
	if ((argc != 2) ) 
	{
		printf("Usage: %s <dev> \n", argv[0]);
		return -1;
	}


	/* 2. 打开文件 */
	fd = open(argv[1], O_RDWR );
	if (fd == -1)
	{
		printf("can not open file %s\n", argv[1]);
		return -1;
	}

	ioctl(fd,IOC_OLED_CLEAR,buf);

	// OLED_DIsp_Char(0,0,'c');
	// OLED_DIsp_Char(8,0,'a');
	// OLED_DIsp_Char(0,2,'s');
	// OLED_DIsp_String(0,4,"liqing!!!");

	for(i = 0;i<38;i++)
	{
		// ioctl(fd,IOC_OLED_CLEAR,buf);
		ioctl(fd,IOC_OLED_KUNKUN,name[i]);
		usleep(50000);
	}
	OLED_DIsp_String(0,4,"liqing!!! ikun");


	close(fd);
	
	return 0;
}


