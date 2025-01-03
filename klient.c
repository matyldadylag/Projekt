#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

static void semafor_v(int semafor_id, int numer_semafora)
{
        struct sembuf bufor_sem;
        bufor_sem.sem_num = numer_semafora;
        bufor_sem.sem_op = 1;
        bufor_sem.sem_flg = SEM_UNDO;

        semop(semafor_id, &bufor_sem, 1);
}

static void semafor_p(int semafor_id, int numer_semafora)
{
        struct sembuf bufor_sem;
        bufor_sem.sem_num = numer_semafora;
        bufor_sem.sem_op = -1;
        bufor_sem.sem_flg = 0;
        
        semop(semafor_id, &bufor_sem, 1);
}

int main(int argc, char *argv[])
{
    key_t key = ftok(".", 28);

    int semafor = semget(key, 1, 0600|IPC_CREAT);

    semafor_p(semafor, 0);
    printf("%d -> basen\n", getpid());
    sleep(rand()%4);
    semafor_v(semafor, 0);
    printf("basen -> %d\n", getpid());

    return 0;
}