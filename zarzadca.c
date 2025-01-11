// TODO Sprawdzic, ktore pliki naglowkowe sa potrzebne
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>

// Zdefiniowane wyżej, aby być w stanie usunąć struktury asynchronicznie w przypadku SIGINT
pid_t PID_kasjera, PID_ratownika;

// Obsluga sygnalu SIGINT
void SIGINT_handler(int sig)
{
    // Zabija kasjera i ratownika, ktorzy dzialaja w petli do momentu dostania SIGINT
    kill(PID_kasjera, SIGINT);
    kill(PID_ratownika, SIGINT);
    
    // Czeka na zakonczenie wszystkich uruchomionych programow (klientow)
    while (wait(NULL) > 0);

    exit(0);
}

int main()
{
    signal(SIGINT, SIGINT_handler);

    // Uruchomienie kasjera
    PID_kasjera = fork();
    if(PID_kasjera == 0)
    {
        execl("./kasjer", "kasjer", NULL);
        exit(0);
    }

    // Uruchomienie ratownika
    PID_ratownika = fork();
    if(PID_ratownika == 0)
    {
        execl("./ratownik", "ratownik", NULL);
        exit(0);
    }

    pid_t PID_klienta;

    // Generowanie klientów
    for(int i = 0; i < 5; i++)
    {
        PID_klienta = fork();
        if(PID_klienta == 0)
        {
            execl("./klient", "klient", NULL);
            exit(0);
        }
    }

    // Czeka na zakończenie wszystkich procesów
    while (wait(NULL) > 0);
}