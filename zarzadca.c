#include "utils.c"

// Definicje globalne
pid_t PID_kasjera, PID_ratownika_brodzik, PID_ratownika_rekreacyjny, PID_ratownika_olimpijski; // Identyfikatory struktur
int ID_pamieci;
time_t* czas_otwarcia; // Zmienne czasu
time_t czas_zamkniecia;
bool czas_przekroczony = false;

// Obsługa sygnału SIGINT
void SIGINT_handler(int sig)
{
    // Wysyła SIGINT do procesów kasjera i ratowników
    kill(PID_kasjera, SIGINT);
    kill(PID_ratownika_brodzik, SIGINT);
    kill(PID_ratownika_rekreacyjny, SIGINT);
    kill(PID_ratownika_olimpijski, SIGINT);
    
    // Usuwa pamięć dzieloną
    shmctl(ID_pamieci, IPC_RMID, 0);
    shmdt(czas_otwarcia);

    // Zarządca czeka na zakończenie pozostałych procesów
    printf("%s[%s] Zarządca czeka, aż wszyscy opuszczą kompleks basenów%s\n", RED, timestamp(), RESET);    
    while (wait(NULL) > 0);
    // Komunikaty o zakończeniu pracy
    printf("%s[%s] Kompleks basenów jest zamknięty%s\n", RED, timestamp(), RESET);
    printf("%s[%s] Zarządca kończy działanie%s\n", RED, timestamp(), RESET);

    exit(0);
}

// Wątek mierzący czas - gdy zostanie osiągnięty czas zamknięcia, wątek wywołuje SIGINT
void* czasomierz()
{
    // Sprawdza, czy został osiągnięty czas zamknięcia
    while (time(NULL) < czas_zamkniecia)
    {
        sleep(1);
    }

    // Ustawia flagę na true
    czas_przekroczony = true;

    // Wyświetla komunikat o osiągnięciu czasu zamknięcia
    printf("%s[%s] Został osiągnięty czas zamknięcia%s\n", RED, timestamp(), RESET);

    return 0;
}

int main()
{
    // Obsługa sygnałów
    signal(SIGINT, SIGINT_handler); // SIGINT wysyłany przez użytkownika lub zarządce po przekroczeniu czasu

    // Komunikat o uruchomieniu zarządcy
    printf("%s[%s] Zarządca uruchomiony\n%s", RED, timestamp(), RESET);

    // Ustalenie maksymalnej liczby klientów
    printf("Podaj maksymalną liczbę klientów: ");
    int maks_klientow;
    scanf("%d", &maks_klientow);

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
    printf("%s[%s] Kompleks basenów jest otwarty%s\n", RED, timestamp(), RESET);
    czas_zamkniecia = *czas_otwarcia + czas_pracy; // Ustalenie czasu zamknięcia

    // Tworzymy wątek, który monitoruje czas zamknięcia
    pthread_t czas;
    pthread_create(&czas, NULL, czasomierz, NULL);
    
    // Uruchomienie kasjera
    PID_kasjera = fork();
    if(PID_kasjera == 0)
    {
        execl("./kasjer", "kasjer", NULL);
        exit(0);
    }

    // Uruchomienie ratownika brodzika
    PID_ratownika_brodzik = fork();
    if(PID_ratownika_brodzik == 0)
    {
        execl("./ratownik_brodzik", "ratownik_brodzik", NULL);
        exit(0);
    }

    // Uruchomienie ratownika basenu rekreacyjnego
    PID_ratownika_rekreacyjny = fork();
    if(PID_ratownika_rekreacyjny == 0)
    {
        execl("./ratownik_basen_rekreacyjny", "ratownik_basen_rekreacyjny", NULL);
        exit(0);
    }

    // Uruchomienie ratownika basenu olimpijskiego
    PID_ratownika_olimpijski = fork();
    if(PID_ratownika_olimpijski == 0)
    {
        execl("./ratownik_basen_olimpijski", "ratownik_basen_olimpijski", NULL);
        exit(0);
    }

    // Generowanie klientów w losowych odstępach czasu
    pid_t PID_klienta;
    while (maks_klientow > 0 && czas_przekroczony==false)
    {
        PID_klienta = fork();
        if(PID_klienta == 0)
        {
            execl("./klient", "klient", NULL);
            exit(0);
        }
        sleep(rand()%10);
        maks_klientow--;
    }

    // Wyświetlenie komunikatu o przekroczeniu maksymalnej liczby klientów
    if(maks_klientow == 0)
    {
        printf("%s[%s] Została przekroczona maksymalna liczba klientów%s\n", RED, timestamp(), RESET);
    }

    // Zarządca czeka, aż nadejdzie czas zamknięcia (jeśli czas został już przekroczony stanie się to od razu)
    pthread_join(czas, NULL);

    // Zarządca zamyka kompleks basenów i kończy działanie
    raise(SIGINT);

    return 0; 
}