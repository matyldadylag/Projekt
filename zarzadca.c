#include "utils.c"

// Definicje globalne
pid_t PID_kasjera, PID_ratownika_brodzik, PID_ratownika_rekreacyjny, PID_ratownika_olimpijski; // Identyfikatory struktur
int ID_kolejki_ratownik_przyjmuje, ID_kolejki_ratownik_wypuszcza, ID_pamieci;
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

    // Usuwa kolejki komunikatów ratowników
    if(msgctl(ID_kolejki_ratownik_przyjmuje, IPC_RMID, 0)==-1)
    {
        handle_error("msgctl ID_kolejki_ratownik_przyjmuje");
    }
    if(msgctl(ID_kolejki_ratownik_wypuszcza, IPC_RMID, 0)==-1)
    {
        handle_error("msgctl ID_kolejki_ratownik_wypuszcza");
    }

    // Usuwa segment pamięci dzielonej
    if(shmctl(ID_pamieci, IPC_RMID, 0)==-1)
    {
        handle_error("shmctl ID_pamieci");
    }
    if(shmdt(czas_przekroczony)==-1)
    {
        handle_error("shmdt czas_przekroczony");
    }

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
    if(signal(SIGINT, SIGINT_handler)==SIG_ERR) // SIGINT wysyłany przez użytkownika lub zarządce po przekroczeniu czasu
    {
        handle_error("signal SIGINT_handler");
    }

    srand(time(NULL));

    // Komunikat o uruchomieniu zarządcy
    printf("%s[%s] Zarządca uruchomiony\n%s", COLOR1, timestamp(), RESET);

    // Sprawdzenie limitu wywołanych procesów przez łącze komunikacyjne
    FILE *fp = popen("bash -c 'ulimit -u'", "r");
    if(fp == NULL) 
    {
        handle_error("popen");
    }

    // Ustalenie maksymalnej liczby klientów
    int maks_klientow = 0;
    int maks_procesy = 0;
    printf("Podaj maksymalną liczbę klientów: ");
    if(scanf("%d", &maks_klientow) != 1) // Użytkownik podaje maksymalną liczbę klientów
    {
        printf("scanf maks_klientow - podano złą wartość\n");
        exit(EXIT_FAILURE);
    }
    if(maks_klientow<0)
    {
        printf("maks_klientow - podano za małą wartość\n");
        exit(EXIT_FAILURE);
    }
    if(fscanf(fp, "%d", &maks_procesy) != 1)
    {
        pclose(fp);
        printf("scanf maks_procesy\n");
        exit(EXIT_FAILURE);
    }
    pclose(fp);
    // Sprawdzenie czy liczba wywołanych procesów nie będzie większa niż limit (+6 bo tyle procesów wywołuję poza klientami)
    if(maks_klientow + 6 > maks_procesy)
    {
        printf("Przekroczono limit ilości procesów\n");
        exit(EXIT_FAILURE);
    }

    // Obsługa czasu działania programu
    printf("Podaj czas pracy basenu (w sekundach): ");
    int czas_pracy;
    if(scanf("%d", &czas_pracy) != 1) // Użytkownik podaje czas pracy programu
    {
        printf("scanf czas_pracy - podano złą wartość\n");
        exit(EXIT_FAILURE);
    }
    // Komunikat o otwarciu kompleksu basenów
    czas_zamkniecia = time(NULL) + czas_pracy; // Ustalenie czasu zamknięcia
    if(czas_zamkniecia<time(NULL) || czas_pracy < 1)
    {
        printf("czas_zamkniecia - podano za krótki czas pracy\n");
        exit(EXIT_FAILURE);
    }
    printf("%s[%s] Kompleks basenów jest otwarty%s\n", COLOR1, timestamp(), RESET);

    // Utworzenie kolejek komunikatów dla ratowników
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    if(klucz_kolejki_ratownik_przyjmuje==-1)
    {
        handle_error("ftok klucz_kolejki_ratownik_przyjmuje");
    }
    ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_przyjmuje==-1)
    {
        handle_error("msgget ID_kolejki_ratownik_przyjmuje");
    }

    // Utworzenie kolejki do wypuszczania klientów
    key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
    if(klucz_kolejki_ratownik_wypuszcza==-1)
    {
        handle_error("ftok klucz_kolejki_ratownik_wypuszcza");
    }
    ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_wypuszcza==-1)
    {
        handle_error("msgget ID_kolejki_ratownik_wypuszcza");
    }

    // Utworzenie segmentu pamięci dzielonej, która przechowuje zmienną bool czas_przekroczony
    key_t klucz_pamieci = ftok(".", 3213);
    if(klucz_pamieci==-1)
    {
        handle_error("ftok klucz_pamieci");
    }
    ID_pamieci = shmget(klucz_pamieci, sizeof(bool), 0600 | IPC_CREAT);
    if(ID_pamieci==-1)
    {
        handle_error("shmget ID_pamieci");
    }
    czas_przekroczony = (bool*)shmat(ID_pamieci, NULL, 0);
    if (czas_przekroczony == (void*)-1)
    {
        handle_error("shmat czas_przekroczony");
    }
    // Inicjalizacja zmiennej jako "false" - czas nie został przekroczony
    *czas_przekroczony = false;

    // Tworzymy wątek, który monitoruje czas zamknięcia
    pthread_t czas;
    if(pthread_create(&czas, NULL, czasomierz, NULL)!=0)
    {
        handle_error("pthread_create czas");
    }
    
    // Uruchomienie kasjera
    PID_kasjera = fork();
    if(PID_kasjera == -1)
    {
        handle_error("fork PID_kasjera");
    }
    if(PID_kasjera == 0)
    {
        execl("./kasjer", "kasjer", NULL);
        handle_error("execl kasjer");
    }

    // Uruchomienie ratownika brodzika
    PID_ratownika_brodzik = fork();
    if(PID_ratownika_brodzik == -1)
    {
        handle_error("fork PID_ratownika_brodzik");
    }
    if(PID_ratownika_brodzik == 0)
    {
        execl("./ratownik_brodzik", "ratownik_brodzik", NULL);
        handle_error("execl ratownik_brodzik");
    }

    // Uruchomienie ratownika basenu rekreacyjnego
    PID_ratownika_rekreacyjny = fork();
    if(PID_ratownika_rekreacyjny == -1)
    {
        handle_error("fork PID_ratownika_rekreacyjny");
    }
    if(PID_ratownika_rekreacyjny == 0)
    {
        execl("./ratownik_basen_rekreacyjny", "ratownik_basen_rekreacyjny", NULL);
        handle_error("execl ratownik_basen_rekreacyjny");
    }

    // Uruchomienie ratownika basenu olimpijskiego
    PID_ratownika_olimpijski = fork();
    if(PID_ratownika_olimpijski == -1)
    {
        handle_error("fork PID_ratownika_olimpijski");
    }
    if(PID_ratownika_olimpijski == 0)
    {
        execl("./ratownik_basen_olimpijski", "ratownik_basen_olimpijski", NULL);
        handle_error("execl ratownik_basen_olimpijski");
    }

    // Generowanie klientów w losowych odstępach czasu, dopóki nie zostanie przekroczona maksymalna liczba klientów lub czas
    while (maks_klientow > 0 && *czas_przekroczony==false)
    {
        pid_t PID_klienta = fork();
        if(PID_klienta == -1)
        {
            handle_error("fork PID_klienta");
        }
        if(PID_klienta == 0)
        {
            execl("./klient", "klient", NULL);
            handle_error("execl klient");
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
    if(pthread_join(czas, NULL) == -1)
    {
        handle_error("pthread_join czas");
    }

    // Zarządca zamyka kompleks basenów i kończy działanie
    raise(SIGINT);

    return 0; 
}