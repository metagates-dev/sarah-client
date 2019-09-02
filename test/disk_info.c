/**
 * disk information fetch test program
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include <sys/fcntl.h>

int main(int argc, char *argv[])
{
    struct hd_driveid drive = {0};
    char *disk_name = "/dev/sda2";
    char serial_no[21] = {'\0'};
    char model[41] = {'\0'};

    int fd = open(disk_name, O_RDONLY);
    if ( fd == -1 ) {
        printf("fail to open the disk %s\n", disk_name);
        goto error;
    }

    if ( ioctl(fd, HDIO_GET_IDENTITY, &drive) == -1  ) {
        printf("fail to invoke the ioctl\n");
        goto error;
    }

    memcpy(serial_no, drive.serial_no, 20);
    memcpy(model, drive.model, 40);

    printf("serial_no=%s\n", serial_no);
    printf("model=%s\n", model);
    printf("wwn=0x%04x%04x%04x%04x\n", drive.words104_125[4], drive.words104_125[5], drive.words104_125[6], drive.words104_125[7]);


error:
    if ( fd > -1 ) {
        close(fd);
    }

    return 0;
}
