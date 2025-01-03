#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>

int main()
{
    int i;

    key_t key = ftok(".", 28);

    int semafor = semget(key, 1, 0600|IPC_CREAT);

    semctl(semafor, 0, SETVAL, 5);

    printf("Stworzono semafor %d\n", semafor);

    for(i = 0; i<10; i++)
    {
        sleep(rand()%4);
        if (fork() == 0)
        {
            execl("./klient", "klient", NULL);
            exit(0);
        }
    }

    for(i = 0; i<10; i++)
    {
        wait(NULL);
    }

    semctl(semafor, 0, IPC_RMID);
    printf("Semafor usunięty\n");

    return 0;
}