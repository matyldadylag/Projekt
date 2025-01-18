#include "utils.c"

// Zdefiniowane globalnie, aby SIGINT_handler mógł usunąć struktury asynchronicznie
pid_t PID_kasjera, PID_ratownika_olimpijski, PID_ratownika_rekreacyjny, PID_ratownika_brodzik;
int ID_pamieci;
time_t* czas_otwarcia;

// Obsługa sygnału SIGINT
void SIGINT_handler(int sig)
{
    // Wysyła SIGINT do procesów kasjera i ratowników
    kill(PID_kasjera, SIGINT);
    kill(PID_ratownika_olimpijski, SIGINT);
    kill(PID_ratownika_rekreacyjny, SIGINT);
    kill(PID_ratownika_brodzik, SIGINT);
    
    // Usuwa pamięć dzieloną
    shmctl(ID_pamieci, IPC_RMID, 0);
    shmdt(czas_otwarcia);

    printf("[%s] Zarządca czeka, aż wszyscy opuszczą kompleks basenów\n", timestamp());    

    while (wait(NULL) > 0);

    printf("[%s] Kompleks basenów jest zamknięty\n", timestamp());

    printf("[%s] Zarządca zakończył pracę\n", timestamp());

    exit(0);
}

int main()
{
    // Obsługa sygnału SIGINT
    signal(SIGINT, SIGINT_handler);

    // Komunikat o uruchomieniu zarządcy
    printf("[%s] Zarządca uruchomiony\n");

    // Obsługa czasu działania programu
    // Utworzenie pamięci dzielonej do przechowywania zmiennej czas_otwarcia
    key_t klucz_pamieci = ftok(".", 3213);
    ID_pamieci = shmget(klucz_pamieci, sizeof(time_t), 0600 | IPC_CREAT);
    czas_otwarcia = (time_t*)shmat(ID_pamieci, NULL, 0);
    printf("Podaj czas pracy basenu (w sekundach): ");
    int czas_pracy;
    scanf("%d", &czas_pracy); // Użytkownik podaje czas pracy programu
    // Komunikat o otwarciu kompleksu basenów
    *czas_otwarcia = time(NULL); // Ustawienie czasu otwarcia, do którego inne procesy będą miały dostęp w pamięci dzielonej
    printf("[%s] Kompleks basenów jest otwarty\n", timestamp());
    time_t czas_zamkniecia = *czas_otwarcia + czas_pracy; // Ustalenie czasu zamknięcia
    
    // Uruchomienie kasjera
    PID_kasjera = fork();
    if(PID_kasjera == 0)
    {
        execl("./kasjer", "kasjer", NULL);
        exit(0);
    }

    // Uruchomienie ratownika basenu olimpijskiego
    PID_ratownika_olimpijski = fork();
    if(PID_ratownika_olimpijski == 0)
    {
        execl("./ratownik_basen_olimpijski", "ratownik_basen_olimpijski", NULL);
        exit(0);
    }

    // Uruchomienie ratownika basenu rekreacyjnego
    PID_ratownika_rekreacyjny = fork();
    if(PID_ratownika_rekreacyjny == 0)
    {
        execl("./ratownik_basen_rekreacyjny", "ratownik_basen_rekreacyjny", NULL);
        exit(0);
    }

    // Uruchomienie ratownika brodzika
    PID_ratownika_brodzik = fork();
    if(PID_ratownika_brodzik == 0)
    {
        execl("./ratownik_brodzik", "ratownik_brodzik", NULL);
        exit(0);
    }

    // Generowanie klientów w losowych odstępach czasu
    pid_t PID_klienta;
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

    // Gdy nadejdzie czas zamknięcia, zarządca zamyka kompleks basenów i kończy działanie
    raise(SIGINT);

    return 0; 
}