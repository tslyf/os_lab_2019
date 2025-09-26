#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork failed");
        return 1;
    }

    if (pid == 0) {
        printf("Child process is starting sequential_min_max...\n");
        fflush(stdout);

        char *args[] = {"./sequential_min_max", "42", "10000", NULL};
        execvp(args[0], args);

        perror("execvp failed");
        exit(1);

    } else {
        printf("Parent process is waiting for the child to finish.\n");
        wait(NULL);
        printf("Child process finished.\n");
    }

    return 0;
}