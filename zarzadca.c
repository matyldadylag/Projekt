#include "header.h"

// Zdefiniowane globalnie, aby SIGINT_handler mógł usunąć struktury asynchronicznie
pid_t PID_kasjera, PID_ratownika_olimpijski;
int ID_pamieci;
time_t* czas_otwarcia;

// Obsługa sygnału SIGINT
void SIGINT_handler(int sig)
{
    // Wysyła SIGINT do procesów kasjera i ratownika
    kill(PID_kasjera, SIGINT);
    kill(PID_ratownika_olimpijski, SIGINT);
    
    shmctl(ID_pamieci, IPC_RMID, 0);
    shmdt(czas_otwarcia);

    exit(0);
}

int main()
{
    // Obsługa sygnału SIGINT
    signal(SIGINT, SIGINT_handler);

    // Utworzenie pamięci dzielonej do przechowywania zmiennej czas_otwarcia
    key_t klucz_pamieci = ftok(".", 3213);
    ID_pamieci = shmget(klucz_pamieci, sizeof(time_t), 0600 | IPC_CREAT);
    czas_otwarcia = (time_t*)shmat(ID_pamieci, NULL, 0);

    // Ustawienie czasu otwarcia, do którego inne procesy będą miały dostęp
    *czas_otwarcia = time(NULL);

    // Uruchomienie kasjera
    PID_kasjera = fork();
    if(PID_kasjera == 0)
    {
        execl("./kasjer", "kasjer", NULL);
        exit(0);
    }

    // Uruchomienie ratownika
    PID_ratownika_olimpijski = fork();
    if(PID_ratownika_olimpijski == 0)
    {
        execl("./ratownik_basen_olimpijski", "ratownik_basen_olimpijski", NULL);
        exit(0);
    }

    // Generowanie klientów w losowych odstępach czasu
    pid_t PID_klienta;

    time_t czas_zamkniecia = *czas_otwarcia + CZAS_PRACY;

    while (time(NULL) < czas_zamkniecia)
    {
        PID_klienta = fork();
        if(PID_klienta == 0)
        {
            execl("./klient", "klient", NULL);
            exit(0);
        }
        sleep(rand()%10);
    }

    sleep(60);

    kill(PID_kasjera, SIGINT);
    kill(PID_ratownika_olimpijski, SIGINT);
    shmctl(ID_pamieci, IPC_RMID, 0);
    shmdt(czas_otwarcia);

    while (wait(NULL) > 0);

    return 0; 
}