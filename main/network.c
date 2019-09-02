/**
 * sarah network statistics interface implementation
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "sarah.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>        //for struct ifreq

SARAH_API sarah_net_stat_t *new_net_stat_entry()
{
    sarah_net_stat_t *net = (sarah_net_stat_t *) sarah_malloc(sizeof(sarah_net_stat_t));
    if ( net == NULL ) {
        SARAH_ALLOCATE_ERROR("new_net_stat_entry", sizeof(sarah_net_stat_t));
    }

    memset(net, 0x00, sizeof(sarah_net_stat_t));
    return net;
}

SARAH_API void copy_net_stat_entry(sarah_net_stat_t *dst, sarah_net_stat_t *src)
{
    memcpy(dst, src, sizeof(sarah_net_stat_t));
}

SARAH_API void free_net_stat_entry(void **ptr)
{
    sarah_net_stat_t **net = (sarah_net_stat_t **) ptr;
    if ( *net != NULL ) {
        sarah_free(*net);
        *ptr = NULL;
    }
}

SARAH_API int sarah_get_net_stat(cel_link_t *list)
{
    FILE *fd;
    char buff[256] = {'\0'};
    unsigned long int tmp = 0;
    sarah_net_stat_t *n;

    fd = fopen("/proc/net/dev", "r");
    if ( fd == NULL ) {
        return 1;
    }

    // ignore the header line
    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        fclose(fd);
        return 2;
    }

    // ignore the title line
    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        fclose(fd);
        return 2;
    }

    while ( 1 ) {
        if ( fgets(buff, sizeof(buff), fd) == NULL ) {
            break;
        }

        // printf("buff=%s\n", buff);

        /*
         * create a new net entry
         * initialized it and then append it to the list
        */
        n = new_net_stat_entry();
        sscanf(
            buff,
            "%47[^:]:%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld", 
            n->device, &n->receive, &n->r_packets,
            &tmp, &tmp, &tmp, &tmp, &tmp, &tmp,
            &n->transmit, &n->t_packets
        );

        cel_left_trim(n->device);

        // get the hd adress
        if ( sarah_get_hardware_addr(n->hd_addr, n->device) != 0 ) {
            // Note: Fail to get the network address
        }

        // append the statistics entry
        cel_link_add_last(list, n);
    }

    fclose(fd);
    return 0;
}

SARAH_API void sarah_print_net_stat(const sarah_net_stat_t *n)
{
    printf(
        "device=%s, hd_addr=%02x:%02x:%02x:%02x:%02x:%02x",
        n->device, 
        n->hd_addr[0], n->hd_addr[1], n->hd_addr[2],
        n->hd_addr[3], n->hd_addr[4], n->hd_addr[5]
    );

    printf(
        ", receive=%ld, r_packets=%ld, transmit=%ld, t_packets=%ld\n",
        n->receive, n->r_packets, n->transmit, n->t_packets
    );
}

/**
 * get the hardware address of the specified network device
 *
 * @param   mac
 * @param   device
*/
SARAH_API int sarah_get_hardware_addr(unsigned char mac[6], char *device)
{
    int sock, ret_code;
    struct ifreq ifreq;

    ret_code = 0;
    if ( (sock = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        return 1;
    }

    strcpy(ifreq.ifr_name, device);
    if ( ioctl(sock, SIOCGIFHWADDR, &ifreq) < 0 ) {
        goto failed;
        ret_code = 1;
    }

    memcpy(mac, &ifreq.ifr_hwaddr.sa_data, 6);
    // mac[0] = ifreq.ifr_hwaddr.sa_data[0];
    // mac[1] = ifreq.ifr_hwaddr.sa_data[1];
    // mac[2] = ifreq.ifr_hwaddr.sa_data[2];
    // mac[3] = ifreq.ifr_hwaddr.sa_data[3];
    // mac[4] = ifreq.ifr_hwaddr.sa_data[4];
    // mac[5] = ifreq.ifr_hwaddr.sa_data[5];

failed: 
    close(sock);

    return ret_code;
}

// Print the mac address to the std string style
// like %X:%X:%X:%X:%X:%X
SARAH_API void sarah_hardware_addr_print(unsigned char mac[6], char *buffer)
{
    snprintf(
        buffer, 18, 
        "%02x:%02x:%02x:%02x:%02x:%02x", 
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
    );
}



SARAH_API sarah_bandwidth_stat_t *new_bandwidth_stat_entry()
{
    sarah_bandwidth_stat_t *bd = (sarah_bandwidth_stat_t *) 
        sarah_malloc(sizeof(sarah_bandwidth_stat_t));
    if ( bd == NULL ) {
        SARAH_ALLOCATE_ERROR("new_bandwidth_stat_entry", sizeof(sarah_bandwidth_stat_t));
    }

    memset(bd, 0x00, sizeof(sarah_bandwidth_stat_t));
    return bd;
}

SARAH_API void free_bandwidth_stat_entry(void **ptr)
{
    sarah_bandwidth_stat_t **bd = (sarah_bandwidth_stat_t **) ptr;
    if ( *bd != NULL ) {
        sarah_free(*bd);
        *ptr = NULL;
    }
}


// interface to calculate the network bandwidth in BITS
SARAH_API int sarah_get_bandwidth_stat(cel_link_t *list)
{
    return sarah_get_bandwidth_stat_for(list, 1);
}

SARAH_API int sarah_get_bandwidth_stat_for(cel_link_t *list, int duration)
{
    double tv_sec;
    struct timeval tv_s, tv_e;

    cel_link_t l1, l2;
    cel_link_node_t *node1, *node2;

    sarah_net_stat_t *n1, *n2;
    sarah_bandwidth_stat_t *bd;


    // initialize work
    cel_link_init(&l1);
    cel_link_init(&l2);

    gettimeofday(&tv_s, NULL);
    sarah_get_net_stat(&l1);
    sleep(duration);
    sarah_get_net_stat(&l2);
    gettimeofday(&tv_e, NULL);

    // time diff in seconds
    tv_sec = (tv_e.tv_sec - tv_s.tv_sec + (tv_e.tv_usec - tv_s.tv_usec) * 0.000001);


    for ( node1 = l1.head->_next, node2 = l2.head->_next;
        node1 != l1.tail && node2 != l2.tail; 
            node1 = node1->_next, node2 = node2->_next ) {

        n1 = (sarah_net_stat_t *) node1->value;
        n2 = (sarah_net_stat_t *) node2->value;

        // create a bandwidth entry
        bd = new_bandwidth_stat_entry();
        copy_net_stat_entry(&bd->round_1, n1);
        copy_net_stat_entry(&bd->round_2, n2);
        bd->incoming = (n2->receive - n1->receive) / tv_sec;
        bd->outgoing = (n2->transmit - n2->transmit) / tv_sec;
        
        cel_link_add_last(list, bd);
    }


    // clean up work
    sarah_clear_link_entries(&l1, free_net_stat_entry);
    sarah_clear_link_entries(&l2, free_net_stat_entry);

    cel_link_destroy(&l1, NULL);
    cel_link_destroy(&l2, NULL);

    return 0;
}

SARAH_API void sarah_print_bandwidth_stat(sarah_bandwidth_stat_t *bd)
{
    sarah_print_net_stat(&bd->round_1);
    sarah_print_net_stat(&bd->round_2);
    printf("Incoming=%f, Outgoing=%f\n", bd->incoming, bd->outgoing);
}
