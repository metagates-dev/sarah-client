#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static int
getdiskid (char *hardc)
{
    int fd;
    struct hd_driveid hid;
    char *wwn;

    fd = open ("/dev/sda", O_RDONLY);
    if (fd < 0)
    {
        perror("Error...");
        return -1;
    }
    if (ioctl(fd, HDIO_GET_IDENTITY, &hid) < 0)
    {
        return -1;
    }

    printf("Model=%.40s\n", hid.model);
    printf("Serial=%.20s\n", hid.serial_no);
    printf("0x%04x%04x%04x%04x",hid.words104_125[4],hid.words104_125[5],hid.words104_125[6],hid.words104_125[7]);
    return 0;
}

int main(void)
{

    // "parted -s /dev/sde mklabel gpt"
    char cmd[80];

    sprintf(cmd, "parted -s %s gpt", "/dev/sde");
    printf("%s\n", cmd);
    return 0;
}