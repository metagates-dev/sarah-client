/**
 * Bash execution test c program
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


int main(int argc, char *argv[])
{
    int no, cpid;
    FILE *fd;
    char buffer[1024];
    char *exec_argv[] = {"ls", "-l", "/etc/sarah/", NULL};

    // simple basic command execution
    no = system("ls -al /etc/sarah/");
    printf("Simple command, result=%d\n", no);
    printf("+------ EOF ------+\n\n");

    // execute a bash file
    no = system("bash ./test.sh");
    printf("Bash File, result=%d\n", no);
    printf("+------ EOF ------+\n\n");

    // popen simple command execution
    fd = popen("cat /etc/sarah/config.json", "r");
    while ( 1 ) {
        if ( fgets(buffer, sizeof(buffer), fd) == NULL ) {
            break;
        }

        printf("%s", buffer);
    }
    pclose(fd);
    printf("+------ EOF ------+\n\n");

    // popen bash file execution
    fd = popen("bash ./test.sh", "r");
    while ( 1 ) {
        if ( fgets(buffer, sizeof(buffer), fd) == NULL ) {
            break;
        }

        printf("%s", buffer);
    }
    pclose(fd);
    printf("+------ EOF ------+\n\n");


    /*
     * exec serial function
    */
    if ( fork() == 0 ) {    // child process
        printf("child process ... \n");
        if ( (no = execv("/bin/ls", exec_argv)) < 0 ) {
            perror("error no exec");
            exit(0);
        }

        // printf("exec result=%d\n", no);
        // while ( 1 ) ;
    } else {
        no = wait(&cpid);
        printf("exec done with parent id=%d\n", no);
    }


    return 0;
}
