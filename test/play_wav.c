/**
 * wav audio file play sample
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
 * @date    2019/03/10
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>


static int play_wav(const char *wav_file)
{
    int errno, arg, fd = -1, dev = -1;
    ssize_t size;
    char buffer[4096] = {'\0'};

    if ( (fd = open(wav_file, O_RDONLY)) < 0 ) {
        errno = 1;
        goto failed;
    }

    if ( (dev = open("/dev/snd/controlC0", O_WRONLY)) < 0 ) {
        errno = 2;
        goto failed;
    }

    // set the arguments for the devices
    arg = 16000;
    if ( ioctl(dev, SOUND_PCM_WRITE_RATE, &arg) == -1 ) {
        errno = 3;
        goto failed;
    }

    arg = 2;
    if ( ioctl(dev, SOUND_PCM_WRITE_CHANNELS, &arg) == -1 ) {
        errno = 4;
        goto failed;
    }

    arg = 16;   // 16bits
    if ( ioctl(dev, SOUND_PCM_WRITE_BITS, &arg) == -1 ) {
        errno = 5;
        goto failed;
    }


    // keep write the stream to the devices
    while ( (size = read(fd, buffer, sizeof(buffer))) > 0 ) {
        if ( write(fd, buffer, size) == -1 ) {
            errno = 5;
            goto failed;
        }
    }

failed:
    if ( fd > -1 ) {
        close(fd);
    }

    if ( dev > -1 ) {
        close(dev);
    }

    return errno;
}

int main(int argc, char *argv[])
{
    int errno;
    errno = play_wav("/home/lionsoul/weather.wav");
    printf("[Info]: finished playing with errno=%d\n", errno);
    return 0;
}
