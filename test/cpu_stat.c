/**
 * cpu usage test
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct cpu_stat_entry
{
    char name[20];
    unsigned int user;      // user cpu time
    unsigned int nice;      // nice cpu time
    unsigned int system;    // system cpu time
    unsigned int idle;      // wait cput time except IO
} cpu_stat_t;


// internal interface to caculate the cpu occupy
float cal_cpu_occupy(const cpu_stat_t *p1, const cpu_stat_t *p2)
{
    long double p1_cost, p2_cost;
    long double u1_cost, u2_cost; 

    // cal the cpu time of p1 and p2
    p1_cost = p1->user + p1->nice + p1->system + p1->idle;
    p2_cost = p2->user + p2->nice + p2->system + p2->idle;

    if ( p2_cost - p1_cost == 0 ) {
        return 0.0;
    }

    // cal user and the system cpu time diff 
    u1_cost = p1->user + p1->nice + p1->system;
    u2_cost = p2->user + p2->nice + p2->system;
    return (float)( ((u2_cost - u1_cost) / (p2_cost - p1_cost)) * 100 );
}

// internal function to get the cpu stat info
int get_cpu_stats(cpu_stat_t *p)
{
    FILE *fd;
    char buff[256] = {'\0'};

    fd = fopen("/proc/stat", "r");
    if ( fd == NULL ) {
        return 1;
    }

    fgets(buff, sizeof(buff), fd);
    sscanf(buff, "%s %u %u %u %u", p->name, &p->user, &p->nice, &p->system, &p->idle);

    fclose(fd);
    return 0;
}

void print_cpu_stats(cpu_stat_t *p)
{
    printf("name=%s, user=%u, nice=%u, system=%u, idle=%u\n", 
        p->name, p->user, p->nice, p->system, p->idle);
}

int main(int argc, char **argv)
{
    // long double a[4], b[4], loadavg;
    // FILE *fp;
    // char dump[50];

    // for(;;)
    // {
    //     fp = fopen("/proc/stat","r");
    //     fscanf(fp,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
    //     fclose(fp);
    //     sleep(1);

    //     fp = fopen("/proc/stat","r");
    //     fscanf(fp,"%*s %Lf %Lf %Lf %Lf",&b[0],&b[1],&b[2],&b[3]);
    //     fclose(fp);

    //     loadavg = ((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3]));
    //     printf("The current CPU utilization is : %Lf\n",loadavg);
    // }

    cpu_stat_t p1;
    cpu_stat_t p2;
    float loadavg;
    register int counter = 0;

    while ( counter++ < 1000 ) {
        get_cpu_stats(&p1);     // get the cpu usage
        sleep(1);
        get_cpu_stats(&p2);     // get the cpu usage
        loadavg = cal_cpu_occupy(&p1, &p2);
        printf("cpu usage: %f\n", loadavg);
    }

    return 0;
}
