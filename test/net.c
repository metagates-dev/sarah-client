/**
 * network detect test
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

typedef struct net_pack_entry
{
    char device[32];                // device name
    unsigned long int receive;      // receive bytes
    unsigned long int r_packets;    // receive packets
    unsigned long int transmit;     // transmit bytes
    unsigned long int t_packets;    // transmit packets
} net_pack_t;

// internal function to print the current net stat information
void print_net_stat(net_pack_t *n)
{
    printf(
        "device=%s, receive=%ld, r_packets=%ld, transmit=%ld, t_packets=%ld\n",
        n->device, n->receive, n->r_packets, n->transmit, n->t_packets
    );
}

// internal function to get the current net stat information
int get_net_stat(net_pack_t *n)
{
    char buff[256] = {'\0'};
    FILE *fd;
    unsigned long int tmp = 0;

    fd = fopen("/proc/net/dev", "r");
    if ( fd == NULL ) {
        return 1;
    }

    // ignore the header line
    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        return 2;
    }

    // ignore the title line
    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        return 2;
    }

    while ( 1 ) {
        if ( fgets(buff, sizeof(buff), fd) == NULL ) {
            break;
        }

        sscanf(
            buff,
            "%s %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld", 
            n->device, &n->receive, &n->r_packets,
            &tmp, &tmp, &tmp, &tmp, &tmp, &tmp,
            &n->transmit, &n->t_packets
        );

        // print_net_stat(n);
    }

    fclose(fd);
    return 0;
}

int main(int argc, char **argv)
{
    net_pack_t n1, n2;
    struct timeval tv_s, tv_e;
    double recv_rate, tran_rate, tv_sec;
    char in_unit[8] = {'\0'}, out_unit[8] = {'\0'};

    while (1) {
        gettimeofday(&tv_s, NULL);
        get_net_stat(&n1);
        sleep(1);
        get_net_stat(&n2);
        gettimeofday(&tv_e, NULL);

        // time diff in seconds
        tv_sec = (tv_e.tv_sec - tv_s.tv_sec + (tv_e.tv_usec - tv_s.tv_usec) * 0.000001);

        // Incoming and Outgoing rate in kb/s
        recv_rate = (n2.receive - n1.receive) / tv_sec * 8;
        if ( recv_rate < 1024) {
            strcpy(in_unit, "Bit/s");
        } else if ( recv_rate < 1048576 ) {                // 1024 * 1024 bytes
            strcpy(in_unit, "KBit/s");
            recv_rate = recv_rate / 1024;
        } else if ( recv_rate < 1073741824 ) {      // 1024 * 1024 * 1024 bytes
            strcpy(in_unit, "MBit/s");
            recv_rate = recv_rate / 1048576;
        } else {
            strcpy(in_unit, "GBit/s");
            recv_rate = recv_rate / 1073741824;
        }

        tran_rate = (n2.transmit - n1.transmit) / tv_sec * 8;
        if ( tran_rate < 1024) {
            strcpy(out_unit, "Bit/s");
        } else if ( tran_rate < 1048576 ) {                // 1024 * 1024 bytes
            strcpy(out_unit, "KBit/s");
            tran_rate = tran_rate / 1024;
        } else if ( tran_rate < 1073741824 ) {      // 1024 * 1024 * 1024 bytes
            strcpy(out_unit, "MBit/s");
            tran_rate = tran_rate / 1048576;
        } else {
            strcpy(out_unit, "GBit/s");
            tran_rate = tran_rate / 1073741824;
        }

        system("clear");
        printf("Bandwidth monitor\n");
        printf("Device        Incoming        Outgoing\n");
        printf(
            "%-12s%6.2f%s%10.2f%s\n", 
            n2.device, recv_rate, in_unit, tran_rate, out_unit
        );
    }

    return 0;
}
