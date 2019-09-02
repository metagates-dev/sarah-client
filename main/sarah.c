/**
 * sarah application startup implementation
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>

#include "sarah.h"
#include "april.h"


static int init_proc_title(int, char **);
static void set_proc_title(int, char **, const char *);

#ifdef _SARAH_GHOST_UDISK_ENABLE
static int ghost_udisk_filter(sarah_disk_t *, int);
static int get_ghost_udisk(cel_link_t *, sarah_disk_t *);
#endif

#define sarah_booter_pid_file "sarah.pid"
#define sarah_booter_exe_file "/usr/local/bin/sarah-booter"
SARAH_API int sarah_booter_init(sarah_booter_t *booter, char *base_dir)
{
    int url_len = 0, errno = 0, pid, _counter;

    memset(booter, 0x00, sizeof(sarah_booter_t));
    booter->status = SARAH_PROC_READY;
    booter->pid = 0;
    booter->base_dir = NULL;
    booter->pid_file = NULL;
    booter->exe_file = sarah_booter_exe_file;

    
    /* Initialization */
    booter->base_dir = strdup(base_dir);

    url_len  = strlen(base_dir);
    url_len += strlen(sarah_booter_pid_file);
    booter->pid_file = (char *) sarah_malloc(url_len + 1);
    if ( booter->pid_file == NULL ) {
        errno = 1;
        goto failed;
    }

    sprintf(booter->pid_file, "%s%s", booter->base_dir, sarah_booter_pid_file);
    if ( sarah_mkdir(booter->base_dir, 
        S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0 ) {
        errno = 2;
        goto failed;
    }

    // printf("base_dir=%s, pid_file=%s\n", booter->base_dir, booter->pid_file);
    // if ( sarah_booter_setpid(booter, 0) != 0 ) {
    //     errno = 4;
    //     goto failed;
    // }
    pid = sarah_booter_getpid(booter);
    if ( pid > 0 ) {
        for ( _counter = 0; _counter < 3; _counter++ ) {
            if ( kill(pid, SARAH_SIGNAL_CHECK) == 0 ) {
                break;
            }
        }

        /* 
         * Basically the process is not running, we here we 
         * consider the process is not running and just ignore it
        */
        if ( _counter < 3 ) {
            errno = 3;
            goto failed;
        }
    }


failed:
    if ( errno > 0 ) {
        if ( booter->base_dir != NULL ) {
            sarah_free(booter->base_dir);
            booter->base_dir = NULL;
        }

        if ( booter->pid_file != NULL ) {
            sarah_free(booter->pid_file);
            booter->pid_file = NULL;
        }
    }

    return errno;
}

SARAH_API void sarah_booter_destroy(sarah_booter_t *booter)
{
    if ( booter->pid_file != NULL ) {
        sarah_free(booter->pid_file);
        booter->pid_file = NULL;
    }

    if ( booter->base_dir != NULL ) {
        free(booter->base_dir);
        booter->base_dir = NULL;
    }
}

SARAH_API pid_t sarah_booter_getpid(sarah_booter_t *booter)
{
    char buffer[12] = {'\0'};
    return sarah_file_get_buffer(booter->pid_file, buffer, 11) > -1 ? atoi(buffer) : 0;
}

SARAH_API int sarah_booter_setpid(sarah_booter_t *booter, pid_t pid)
{
    char buffer[12] = {'\0'};
    booter->pid = pid;
    sprintf(buffer, "%d", pid);
    return sarah_file_put_buffer(booter->pid_file, buffer, 11, 0) > -1 ? 0 : 1;
}


/** Application implementation define */
extern char **environ;
static char *last = NULL;
static sarah_booter_t _g_sarah_booter;

/* global variables define */
static int init_proc_title(int argc, char **argv)
{
    int i;
    size_t size = 0;

    for (i = 0; environ[i]; ++i) {
        size += strlen(environ[i])+1;
    }

    char *raw = (char *) sarah_malloc(size);
    if ( raw == NULL ) {
        return 1;
    }

    for (i = 0; environ[i]; ++i) {
        memcpy(raw, environ[i], strlen(environ[i]) + 1);
        environ[i] = raw;
        raw += strlen(environ[i]) + 1;
    }

    last = argv[0];
    for (i = 0; i < argc; ++i) {
        last += strlen(argv[i]) + 1;
    }

    for (i = 0; environ[i]; ++i) {
        last += strlen(environ[i]) + 1;
    }

    return 0;
}

static void set_proc_title(int argc, char **argv, const char *title)
{
    argv[1] = 0;
    char *p = argv[0];
    memset(p, 0x00, last - p);
    strncpy(p, title, last - p);
}

#ifdef _SARAH_GHOST_UDISK_ENABLE
static int ghost_udisk_filter(sarah_disk_t *disk, int step)
{
    switch ( step ) {
    case -1:
        if ( strncmp(disk->name, "/dev/loop", 9) == 0 
            || strncmp(disk->name, "/dev/ram", 8) == 0 ) {
            return 1;
        }
        break;
    case SARAH_DISK_FILL_PT:
        if ( strcmp(disk->format, "vfat") != 0 ) {
            return 1;
        }
        break;
    }

    return 0;
}

/* get the ghost u-disk */
static int get_ghost_udisk(cel_link_t *disk_list, sarah_disk_t *udisk)
{
    int is_match = 0;
    sarah_disk_t *disk;

    // get the DISK list
    sarah_get_disk_list(
        disk_list, SARAH_DISK_FILL_PT | SARAH_DISK_FILL_ST, ghost_udisk_filter
    );

    if ( disk_list->size == 0 ) {
        return 1;
    }

    while ( (disk = cel_link_remove_first(disk_list)) != NULL ) {
        if ( disk->part_no > -1 ) {
            // size bettween 2031616 - 512 and 2031616 + 512
            if ( is_match == 0 
                && disk->p_size >= 2031104 
                    && disk->p_size <= 2032128 ) {
                is_match = 1;
                memcpy(udisk, disk, sizeof(sarah_disk_t));
            }
        }

        free_disk_entry((void **) &disk);
    }

    return is_match == 1 ? 0 : 2;
}
#endif


static sarah_application_t _internal_apps[] = {
    {"sarah-smartdisk", sarah_smartdisk_application, 0},
    {"sarah-collector", sarah_collector_application, 0},
    {"sarah-executor", sarah_executor_application, 0},
    {"sarah-updater", sarah_updater_application, 0},
    {NULL, NULL, 0}
};

/* internal signal to install */
static int _internal_signals[] = {
    SARAH_SIGNAL_STOP, SARAH_SIGNAL_CONT,
    SARAH_SIGNAL_EXIT_01, SARAH_SIGNAL_EXIT_02, 
    SARAH_SIGNAL_CHECK, SARAH_SIGNAL_RESTART, -1
};

/** signal handler function */
static void _signal_handler(int _signal)
{
    switch ( _signal ) {
    case SARAH_SIGNAL_STOP:
        _g_sarah_booter.status = SARAH_PROC_STOP;
        break;
    case SARAH_SIGNAL_CONT:
        _g_sarah_booter.status = SARAH_PROC_RUNNING;
        break;
    case SARAH_SIGNAL_EXIT_01:        // from terminal Ctrl + c
    case SARAH_SIGNAL_EXIT_02:        // from terminal kill or kill function
        _g_sarah_booter.status = SARAH_PROC_EXIT;
        break;
    case SARAH_SIGNAL_RESTART:       
        _g_sarah_booter.status = SARAH_PROC_RESTART;
        break;
    case SARAH_SIGNAL_CHECK:
        break;
    }
}


int main(int argc, char *argv[])
{
    int errno, _counter;
    int _app_idx, _sig_idx, _signal, _exit_status;
    struct timeval tv_s;

#ifdef _SARAH_GHOST_UDISK_ENABLE
    cel_link_t disk_list;       // disk list
    sarah_disk_t ghost_udisk;   // ghost udisk
    int ghost_errno = 0, has_ghost_udisk = 0;
    int sigstop_sended = 0, sigcont_sended = 0;
    char *ghost_errstr, config_file[256] = {'\0'};
#endif

    char *base_dir = "/etc/sarah/";
    sarah_node_t node;
    sarah_application_t *app;

    // --- Basic necessary checking
    if ( init_proc_title(argc, argv) != 0 ) {
        printf("Failed to initialize the process title\n");
        return 0;
    }

    // Make sure the running user is root
    if ( getuid() != 0 ) {
        printf("Please run the command with sudo, that is 'sudo %s'\n", argv[0]);
        return 0;
    }

    // --- Initialize work
    sarah_mkdir(APRIL_PACKAGE_BASE_DIR, 0755);
    if ( (errno = sarah_booter_init(&_g_sarah_booter, base_dir)) != 0 ) {
        printf("[Error]: Failed to initialize the booter with errno=%d\n", errno);
        return 1;
    }

    if ( (errno = sarah_booter_setpid(&_g_sarah_booter, getpid())) != 0 ) {
        printf("[Error]: Failed to set booter pid with errno=%d\n", errno);
        return 1;
    }

    if ( (errno = sarah_node_init(&node, base_dir)) != 0 ) {
        printf("[Error]: Failed to initialize the node with errno=%d\n", errno);
        return 1;
    }


    // Set the last boot time for the node
    // and synchronized it to the local storage media
#ifdef _SARAH_GHOST_UDISK_ENABLE
    cel_link_init(&disk_list);
#endif

    gettimeofday(&tv_s, NULL);
    sarah_node_set_last_boot_at(&node, tv_s.tv_sec, 1);
    if ( node.interval <= 0 || node.interval > 600 ) {
        node.interval = sarah_node_interval;
    }

    // print the basic information of the node
    printf("<<<Running stat: \n");
    printf("|-version: %.2f\n", sarah_version());
    printf("|-node.id: %s\n", node.node_id);
    printf("|-node.is_new: %s\n", node.is_new ? "Yes" : "No");
    printf("|-node.created at: %u\n", node.created_at);
    printf("|-node.last_boot_at: %u\n", node.last_boot_at);
    printf("|-node.base_dir: %s\n", node.base_dir);
    printf(">>>\n\n");


    /* install the signal handler */
    _sig_idx = 0;
    while ( 1 ) {
        _signal = _internal_signals[_sig_idx++];
        if ( _signal < 0 ) {
            break;
        }
        
        if ( signal(_signal, _signal_handler) == SIG_ERR ) {
            printf("|-[Error]: Failed to install signal %d\n", _signal);
        }
    }


    /* loop to start all the application */
    _app_idx = 0;
    while ( 1 ) {
        app = &_internal_apps[_app_idx++];
        if ( app->name == NULL ) {
            break;
        }

        printf("+-Try to start aplication %s ... \n", app->name);
        app->pid = fork();
        if ( app->pid < 0 ) {
            printf("|-[Error]: Failed to fork a new process.\n");
        } else if ( app->pid == 0 ) {
            printf("|-[Info]: Application %s started\n", app->name);
            set_proc_title(argc, argv, app->name);
            app->boot(&_g_sarah_booter, &node);
#ifdef _SARAH_GHOST_UDISK_ENABLE
            cel_link_destroy(&disk_list, NULL);
#endif
            sarah_node_destroy(&node, 0);
            sarah_booter_destroy(&_g_sarah_booter);
            return 0;
        }
    }

    /* 
     * loop to check status of the application 
     * make sure they are running or start it up if any one them is down.
    */
    _app_idx = 0;
    while ( 1 ) {
        /* check and process the restart and the exit signal */
        if ( _g_sarah_booter.status == SARAH_PROC_RESTART 
            || _g_sarah_booter.status == SARAH_PROC_EXIT ) {
            for ( _app_idx = 0; 
                _internal_apps[_app_idx].name != NULL; _app_idx++ ) {
                app = &_internal_apps[_app_idx];
                printf("+-Try to stop process %s ... \n", app->name);
                for ( _counter = 0; _counter < 3; _counter++ ) {
                    if ( kill(app->pid, SARAH_SIGNAL_EXIT_02) == 0 ) {
                        printf("[Info]: %s exit signal sended ... \n", app->name);
                        break;
                    }
                }

                if ( _counter == 3 ) {
                    kill(app->pid, SARAH_SIGNAL_KILL);
                }
            }

            for ( _app_idx = 0; 
                _internal_apps[_app_idx].name != NULL; _app_idx++ ) {
                app = &_internal_apps[_app_idx];
                if ( waitpid(app->pid, NULL, 0) ) {
                    printf("|-[Info]: Process %s stoped.\n", app->name);
                }
            }

            break;
        }

#ifdef _SARAH_GHOST_UDISK_ENABLE
        /* Check if the Ghost u-disk is active */
        ghost_errno = 0;
        if ( has_ghost_udisk == 0 ) {
            if ( get_ghost_udisk(&disk_list, &ghost_udisk) > 0 ) {
                ghost_errno = 1;
            } else {
                has_ghost_udisk = 1;
            }
        }

        if ( has_ghost_udisk == 1 ) {
            config_file[0] = '\0';
            strcat(config_file, ghost_udisk.mount);
            strcat(config_file, "/config.json");
            // printf("config file=%s, app_key=%s\n", config_file, node.config.app_key);
            if ( access(config_file, F_OK) == -1 ) {
                ghost_errno = 2;
            } else if ( sarah_file_contains(config_file, node.config.app_key) == -1 ) {
                ghost_errno = 3;
            }
        }

        /* Error handling */
        if ( ghost_errno > 0 ) {
            switch ( ghost_errno ) {
            case 1:
                ghost_errstr = "Please insert the Ghost u-disk to continue.";
                break;
            case 2:
                has_ghost_udisk = 0;
                ghost_errstr = "Invalid Ghost u-disk for missing config";
                break;
            case 3:
                ghost_errstr = "Invalid Ghost u-disk for config error.";
                break;
            }

            printf("%s\n", ghost_errstr);
            if ( sigstop_sended == 0 ) {
                for ( _sig_idx = 0; 
                    _internal_apps[_sig_idx].name != NULL; _sig_idx++ ) {
                    app = &_internal_apps[_sig_idx];
                    if ( app->name == NULL ) {
                        break;
                    }

                    kill(app->pid, SARAH_SIGNAL_STOP);
                }

                sigcont_sended = 0;
                sigstop_sended = 1;
            }
        } else if ( sigcont_sended == 0 ) {
            for ( _sig_idx = 0; 
                _internal_apps[_sig_idx].name != NULL; _sig_idx++ ) {
                app = &_internal_apps[_sig_idx];
                if ( app->name == NULL ) {
                    break;
                }

                kill(app->pid, SARAH_SIGNAL_CONT);
            }

            sigstop_sended = 0;
            sigcont_sended = 1;
        }
#endif

        /* Check and reboot the process if any of them is down */
        if ( _g_sarah_booter.status == SARAH_PROC_RUNNING ) {
            for ( _app_idx = 0; 
                _internal_apps[_app_idx].name != NULL; _app_idx++ ) {
                app = &_internal_apps[_app_idx++];
                if ( app->name == NULL ) {
                    break;
                }

                /* if the current process is down reboot it */
                if ( waitpid(app->pid, NULL, WNOHANG) > 0 ) {
                    printf("+-[Warning]: %s is down try to reboot it ... \n", app->name);
                    app->pid = fork();
                    if ( app->pid < 0 ) {
                        printf("|-[Error]: Failed to fork a new process.\n");
                    } else if ( app->pid == 0 ) {
                        printf("|-[Info]: Application %s restarted\n", app->name);
                        set_proc_title(argc, argv, app->name);
                        app->boot(&_g_sarah_booter, &node);
#ifdef _SARAH_GHOST_UDISK_ENABLE
                        cel_link_destroy(&disk_list, NULL);
#endif
                        sarah_node_destroy(&node, 0);
                        sarah_booter_destroy(&_g_sarah_booter);
                        return 0;
                    }
                }
            }
        }

        sleep(6);
    }

    // --- resource clean up work
#ifdef _SARAH_GHOST_UDISK_ENABLE
    cel_link_destroy(&disk_list, NULL);
#endif
    sarah_node_destroy(&node, 0);
    _exit_status = _g_sarah_booter.status;
    sarah_booter_setpid(&_g_sarah_booter, 0);
    sarah_booter_destroy(&_g_sarah_booter);
    printf("+[Info]: sarah booter stoped with errno=%d.\n", errno);

    /* Check and restart the booter */
    if ( _exit_status == SARAH_PROC_RESTART ) {
        printf("+-Try to restart the booter=%s ... \n", argv[0]);
        if ( execve(argv[0], argv, environ) < 0 ) {
            exit(0);
        }
    }

    return 0;
}
