#include "utils.c"

// Definicje globalne
pid_t PID_kasjera, PID_ratownika_brodzik, PID_ratownika_rekreacyjny, PID_ratownika_olimpijski; // Identyfikatory struktur
int ID_pamieci;
time_t czas_zamkniecia; // Zmienne dotyczące czasu korzystane przez funkcję main i wątek
bool* czas_przekroczony;

// Obsługa sygnału SIGINT
void SIGINT_handler(int sig)
{
    // Wysyła SIGINT do procesów kasjera i ratowników
    kill(PID_ratownika_brodzik, SIGINT);
    kill(PID_ratownika_rekreacyjny, SIGINT);
    kill(PID_ratownika_olimpijski, SIGINT);
    kill(PID_kasjera, SIGINT);

    // Zarządca czeka na zakończenie pozostałych procesów
    printf("%s[%s] Zarządca czeka, aż wszyscy opuszczą kompleks basenów%s\n", COLOR1, timestamp(), RESET);    
    while (wait(NULL) > 0);

    // Usuwa segment pamięci dzielonej
    shmctl(ID_pamieci, IPC_RMID, 0);
    shmdt(czas_przekroczony);

    // Komunikaty o zakończeniu pracy
    printf("%s[%s] Kompleks basenów jest zamknięty%s\n", COLOR1, timestamp(), RESET);
    printf("%s[%s] Zarządca kończy działanie%s\n", COLOR1, timestamp(), RESET);

    exit(0);
}

// Wątek mierzący czas - gdy zostanie osiągnięty czas zamknięcia, przerywa generowanie klientów
void* czasomierz()
{
    // Sprawdza, czy został osiągnięty czas zamknięcia
    while (time(NULL) < czas_zamkniecia)
    {
        sleep(1);
    }

    // Ustawia flagę na true
    *czas_przekroczony = true;

    // Wyświetla komunikat o osiągnięciu czasu zamknięcia
    printf("%s[%s] Został osiągnięty czas zamknięcia%s\n", COLOR1, timestamp(), RESET);

    return NULL;
}

int main()
{
    // Obsługa sygnałów
    signal(SIGINT, SIGINT_handler); // SIGINT wysyłany przez użytkownika lub zarządce po przekroczeniu czasu

    srand(time(NULL));

    // Komunikat o uruchomieniu zarządcy
    printf("%s[%s] Zarządca uruchomiony\n%s", COLOR1, timestamp(), RESET);

    // Ustalenie maksymalnej liczby klientów
    printf("Podaj maksymalną liczbę klientów: ");
    int maks_klientow;
    scanf("%d", &maks_klientow); // Użytkownik podaje maksymalną liczbę klientów

    // Obsługa czasu działania programu
    printf("Podaj czas pracy basenu (w sekundach): ");
    int czas_pracy;
    scanf("%d", &czas_pracy); // Użytkownik podaje czas pracy programu
    // Komunikat o otwarciu kompleksu basenów
    printf("%s[%s] Kompleks basenów jest otwarty%s\n", COLOR1, timestamp(), RESET);
    czas_zamkniecia = time(NULL) + czas_pracy; // Ustalenie czasu zamknięcia

    // Utworzenie segmentu pamięci dzielonej, która przechowuje zmienną bool czas_przekroczony
    key_t klucz_pamieci = ftok(".", 3213);
    ID_pamieci = shmget(klucz_pamieci, sizeof(bool), 0600 | IPC_CREAT);
    czas_przekroczony = (bool*)shmat(ID_pamieci, NULL, 0);
    // Inicjalizacja zmiennej jako "false" - czas nie został przekroczony
    *czas_przekroczony = false;

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

    // Generowanie klientów w losowych odstępach czasu, dopóki nie zostanie przekroczona maksymalna liczba klientów lub czas
    while (maks_klientow > 0 && *czas_przekroczony==false)
    {
        pid_t PID_klienta = fork();
        if(PID_klienta == 0)
        {
            execl("./klient", "klient", NULL);
            exit(0);
        }
        sleep(rand()%3);
        maks_klientow--;
    }

    // Wyświetlenie komunikatu o przekroczeniu maksymalnej liczby klientów
    if(maks_klientow == 0)
    {
        printf("%s[%s] Została przekroczona maksymalna liczba klientów%s\n", COLOR1, timestamp(), RESET);
    }

    // Zarządca czeka, aż nadejdzie czas zamknięcia (jeśli czas został już przekroczony stanie się to od razu)
    pthread_join(czas, NULL);

    // Zarządca zamyka kompleks basenów i kończy działanie
    raise(SIGINT);

    return 0; 
}