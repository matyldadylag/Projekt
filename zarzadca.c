#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <stdlib.h>
#include <wait.h>

pid_t PID_kasjera;

// Obsluga sygnalu SIGINT
void handle_sigint(int sig)
{
    // Czeka na zakonczenie wszystkich uruchomionych programow
    int status;
    for (int j = 0; j < 11; j++)
    {
        wait(NULL);
    }

    // Zabija kasjera, ktory dziala w petli do momentu dostania SIGINT
    kill(PID_kasjera, SIGINT);

    exit(0);
}

int main()
{
    signal(SIGINT, handle_sigint);

    // Uruchomienie kasjera
    PID_kasjera = fork();
    if(PID_kasjera == 0)
    {
        execl("./kasjer", "kasjer", NULL);
        exit(0);
    }

    // Generowanie klientów
    for(int i = 0; i < 10; i++)
    {
        if (fork() == 0)
        {
                execl("./klient", "klient", NULL);
                exit(0);
        }
        sleep(3);
    }

    while(1)
    {
        printf("Basen jest otwarty\n");
        sleep(5);
    }
}