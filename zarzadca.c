#include "utils.c"

// ID utworzonych struktur
int ID_kolejki_kasjer, ID_kolejki_ratownik_przyjmuje, ID_kolejki_ratownik_wypuszcza, ID_semafora_brodzik, ID_semafora_rekreacyjny, ID_semafora_olimpijski, ID_pamieci_czas_pracy, ID_pamieci_okresowe_zamkniecie;
// PID tworzonych procesów
pid_t PID_kasjera, PID_ratownika_brodzik, PID_ratownika_rekreacyjny, PID_ratownika_olimpijski;
// Zmienne dotyczące czasu wykorzystywane przez funkcję main i wątki
bool czas_przekroczony;
time_t czas_zamkniecia;
// Zmienne dotyczące czasu przechowywane w pamięci dzielonej
int *czas_pracy;
bool *okresowe_zamkniecie;

void SIGINT_handler(int sig);
void* czasomierz();
void* sprzatanie_klientow();

int main()
{
    // Komunikat o uruchomieniu zarządcy
    printf("%s[%s] Zarządca uruchomiony\n%s", COLOR1, timestamp(), RESET);

    // Obsługa sygnału SIGINT, wysłanego przez użytkownika lub zarządcę
    if(signal(SIGINT, SIGINT_handler)==SIG_ERR)
    {
        handle_error("zarzadca: signal SIGINT_handler");
    }

    // Sprawdzenie limitu wywołanych procesów przez łącze komunikacyjne
    FILE *fp = popen("bash -c 'ulimit -u'", "r"); // Otwarcie łącza
    if(fp == NULL) 
    {
        handle_error("zarzadca: popen");
    }
    int maks_procesy = 0;
    if(fscanf(fp, "%d", &maks_procesy) != 1) // Zczytanie wartości wykonania polecenia z pliku fp do zmiennej maks_procesy
    {
        pclose(fp);
        printf("zarzadca: scanf maks_procesy\n");
        exit(EXIT_FAILURE);
    }
    pclose(fp); // Zamknięcie łącza

    // Ustalenie maksymalnej liczby klientów
    int maks_klientow = 0;
    printf("Podaj maksymalną liczbę klientów: ");
    if(scanf("%d", &maks_klientow) != 1) // Użytkownik podaje maksymalną liczbę klientów
    {
        printf("zarzadca: scanf maks_klientow - podano złą wartość\n");
        exit(EXIT_FAILURE);
    }
    if(maks_klientow<0) // Sprawdzenie czy liczba klientów nie jest mniejsza od 0
    {
        printf("zarzadca: maks_klientow - podano za małą wartość\n");
        exit(EXIT_FAILURE);
    }
    if(maks_klientow + 6 > maks_procesy) // Sprawdzenie czy liczba klientów + pozostałych procesów nie przekracza limitu
    {
        printf("zarzadca: przekroczono limit ilości procesów\n");
        exit(EXIT_FAILURE);
    }

    // Obsługa czasu działania programu
    int temp_czas_pracy;
    printf("Podaj czas pracy basenu (w sekundach): ");
    if(scanf("%d", &temp_czas_pracy) != 1) // Użytkownik podaje czas pracy programu
    {
        printf("zarzadca: scanf czas_pracy - podano złą wartość\n");
        exit(EXIT_FAILURE);
    }
    czas_zamkniecia = time(NULL) + temp_czas_pracy; // Ustalenie czasu zamknięcia
    if(czas_zamkniecia<time(NULL)) // Sprawdzenie czy czas zamknięcia jest poprawny i już nie minął
    {
        printf("czas_zamkniecia - podano za krótki czas pracy\n");
        exit(EXIT_FAILURE);
    }

    // Utworzenie potrzebnych struktur
    // Utworzenie kolejki komunikatów dla kasjera
    key_t klucz_kolejki_kasjer = ftok(".", 6377);
    if (klucz_kolejki_kasjer == -1)
    {
        handle_error("zarzadca: ftok klucz_kolejki_kasjer");
    }
    ID_kolejki_kasjer = msgget(klucz_kolejki_kasjer, IPC_CREAT | 0600);
    if (ID_kolejki_kasjer == -1)
    {
        handle_error("zarzadca: msgget ID_kolejki_kasjer");
    }
    
    // Utworzenie kolejki komunikatów dla ratowników - przyjmowanie klientów
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    if(klucz_kolejki_ratownik_przyjmuje==-1)
    {
        handle_error("zarzadca: ftok klucz_kolejki_ratownik_przyjmuje");
    }
    ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_przyjmuje==-1)
    {
        handle_error("zarzadca: msgget ID_kolejki_ratownik_przyjmuje");
    }
    
    // Utworzenie kolejki komunikatów dla ratowników - wypuszczanie klientów
    key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
    if(klucz_kolejki_ratownik_wypuszcza==-1)
    {
        handle_error("zarzadca: ftok klucz_kolejki_ratownik_wypuszcza");
    }
    ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_wypuszcza==-1)
    {
        handle_error("zarzadca: msgget ID_kolejki_ratownik_wypuszcza");
    }
    
    // Utworzenie semafora dla brodzika
    key_t klucz_semafora_brodzik = ftok(".", 3293);
    if(klucz_semafora_brodzik == -1)
    {
        handle_error("zarzadca: ftok klucz_semafora_brodzik");
    }
    ID_semafora_brodzik = semget(klucz_semafora_brodzik, 1, 0600 | IPC_CREAT);
    if(ID_semafora_brodzik == -1)
    {
        handle_error("zarzadca: semget ID_semafora_brodzik");
    }
    if(semctl(ID_semafora_brodzik, 0, SETVAL, MAKS_BRODZIK) == -1)
    {
        handle_error("zarzadca: semctl SETVAL ID_semafora_brodzik");
    }
    
    // Utworzenie semafora dla basenu rekreacyjnego
    key_t klucz_semafora_rekreacyjny = ftok(".", 2003);
    if(klucz_semafora_rekreacyjny == -1)
    {
        handle_error("zarzadca: ftok klucz_semafora_rekreacyjny");
    }
    ID_semafora_rekreacyjny = semget(klucz_semafora_rekreacyjny, 1, 0600 | IPC_CREAT);
    if(ID_semafora_rekreacyjny == -1)
    {
        handle_error("zarzadca: semget ID_semafora_rekreacyjny");
    }
    if(semctl(ID_semafora_rekreacyjny, 0, SETVAL, MAKS_REKREACYJNY) == -1)
    {
        handle_error("zarzadca: semctl SETVAL ID_semafora_rekreacyjny");
    }
    
    // Utworzenie semafora dla basenu olimpijskiego
    key_t klucz_semafora_olimpijski = ftok(".", 9447);
    if(klucz_semafora_olimpijski==-1)
    {
        handle_error("zarzadca: ftok klucz_semafora_olimpijski");
    }
    ID_semafora_olimpijski = semget(klucz_semafora_olimpijski, 1, 0600|IPC_CREAT);
    if(ID_semafora_olimpijski==-1)
    {
        handle_error("zarzadca: semget ID_semafora_olimpijski");
    }
    if(semctl(ID_semafora_olimpijski, 0, SETVAL, MAKS_OLIMPIJSKI)==-1)
    {
        handle_error("zarzadca: semctl ID_semafora_olimpijski");
    }

    // Utworzenie segmentu pamięci dzielonej, która przechowuje zmienną czas_pracy
    key_t klucz_pamieci_czas_pracy = ftok(".", 1400);
    if(klucz_pamieci_czas_pracy==-1)
    {
        handle_error("zarzadca: ftok klucz_pamieci_czas_pracy");
    }
    ID_pamieci_czas_pracy = shmget(klucz_pamieci_czas_pracy, sizeof(int), 0600 | IPC_CREAT);
    if(ID_pamieci_czas_pracy==-1)
    {
        handle_error("zarzadca: shmget ID_pamieci_czas_pracy");
    }
    czas_pracy = (int*)shmat(ID_pamieci_czas_pracy, NULL, 0);
    if (czas_pracy == (void*)-1)
    {
        handle_error("zarzadca: shmat czas_pracy");
    }
    *czas_pracy= temp_czas_pracy;
    
    // Utworzenie segmentu pamięci dzielonej, która przechowuje zmienną bool okresowe_zamkniecie
    key_t klucz_pamieci_okresowe_zamkniecie = ftok(".", 9929);
    if(klucz_pamieci_okresowe_zamkniecie==-1)
    {
        handle_error("zarzadca: ftok klucz_pamieci_okresowe_zamkniecie");
    }
    ID_pamieci_okresowe_zamkniecie = shmget(klucz_pamieci_okresowe_zamkniecie, sizeof(bool), 0600 | IPC_CREAT);
    if(ID_pamieci_okresowe_zamkniecie==-1)
    {
        handle_error("zarzadca: shmget ID_pamieci_okresowe_zamkniecie");
    }
    okresowe_zamkniecie = (bool*)shmat(ID_pamieci_okresowe_zamkniecie, NULL, 0);
    if (okresowe_zamkniecie == (void*)-1)
    {
        handle_error("zarzadca: shmat okresowe_zamkniecie");
    }

    // Komunikat o otwarciu kompleksu basenów
    printf("%s[%s] Kompleks basenów jest otwarty%s\n", COLOR1, timestamp(), RESET);

    // Utworzenie wąteku, który monitoruje czas zamknięcia i czas okresowego zamknięcia
    czas_przekroczony = false;
    pthread_t czas;
    if(pthread_create(&czas, NULL, czasomierz, NULL)!=0)
    {
        handle_error("zarzadca: pthread_create czas");
    }

    // Uruchomienie kasjera
    PID_kasjera = fork();
    if(PID_kasjera == -1)
    {
        handle_error("zarzadca: fork PID_kasjera");
    }
    if(PID_kasjera == 0)
    {
        execl("./kasjer", "kasjer", NULL);
        handle_error("zarzadca: execl kasjer");
    }

    // Uruchomienie ratownika brodzika
    PID_ratownika_brodzik = fork();
    if(PID_ratownika_brodzik == -1)
    {
        handle_error("zarzadca: fork PID_ratownika_brodzik");
    }
    if(PID_ratownika_brodzik == 0)
    {
        execl("./ratownik_brodzik", "ratownik_brodzik", NULL);
        handle_error("zarzadca: execl ratownik_brodzik");
    }

    // Uruchomienie ratownika basenu rekreacyjnego
    PID_ratownika_rekreacyjny = fork();
    if(PID_ratownika_rekreacyjny == -1)
    {
        handle_error("zarzadca: fork PID_ratownika_rekreacyjny");
    }
    if(PID_ratownika_rekreacyjny == 0)
    {
        execl("./ratownik_basen_rekreacyjny", "ratownik_basen_rekreacyjny", NULL);
        handle_error("zarzadca: execl ratownik_basen_rekreacyjny");
    }

    // Uruchomienie ratownika basenu olimpijskiego
    PID_ratownika_olimpijski = fork();
    if(PID_ratownika_olimpijski == -1)
    {
        handle_error("zarzadca: fork PID_ratownika_olimpijski");
    }
    if(PID_ratownika_olimpijski == 0)
    {
        execl("./ratownik_basen_olimpijski", "ratownik_basen_olimpijski", NULL);
        handle_error("zarzadca: execl ratownik_basen_olimpijski");
    }

    // Uruchomienie wątka sprzątającego klientów
    pthread_t sprzatanie;
    if(pthread_create(&sprzatanie, NULL, sprzatanie_klientow, NULL)!=0)
    {
        handle_error("zarzadca: pthread_create sprzatanie");
    }

    // Generowanie klientów w losowych odstępach czasu, dopóki nie zostanie przekroczona maksymalna liczba klientów lub czas
    srand(time(NULL)); // Punkt początkowy dla losowania
    while (maks_klientow>0 && czas_przekroczony==false)
    {
        pid_t PID_klienta = fork();
        if(PID_klienta == -1)
        {
            handle_error("zarzadca: fork PID_klienta");
        }
        else if(PID_klienta == 0)
        {
            execl("./klient", "klient", NULL);
            handle_error("zarzadca: execl klient");
        }
        else
        {
 
        }
        maks_klientow--;
        sleep(rand()%3+1);
    }

    // Wyświetlenie komunikatu o przekroczeniu maksymalnej liczby klientów
    if(maks_klientow == 0)
    {
        printf("%s[%s] Została przekroczona maksymalna liczba klientów%s\n", COLOR1, timestamp(), RESET);
    }

    // Zarządca czeka, aż nadejdzie czas zamknięcia (jeśli czas został już przekroczony stanie się to od razu)
    if(pthread_join(czas, NULL) == -1)
    {
        handle_error("zarzadca: pthread_join czas");
    }

    // Zarządca przyłącza wątek sprzątający klientów
    if(pthread_join(sprzatanie, NULL) == -1)
    {
        handle_error("zarzadca: pthread_join sprzatanie");
    }

    // Zarządca zamyka kompleks basenów i kończy działanie
    raise(SIGINT);

    return 0; 
}

// Obsługa sygnału SIGINT
void SIGINT_handler(int sig)
{
    if(system("killall -s 2 klient 2>/dev/null")==-1) // Zabija wszystkie procesy o nazwie klient (przekierowywuje wyście błędu, bo prawdopodobnie dostanie "permission denied")
    {
        handle_error("zarzadca: system killal klient");
    }

    // Wysyła SIGINT do procesów kasjera i ratowników
    kill(PID_kasjera, SIGINT);
    kill(PID_ratownika_brodzik, SIGINT);
    kill(PID_ratownika_rekreacyjny, SIGINT);
    kill(PID_ratownika_olimpijski, SIGINT);
    
    // Zarządca czeka na zakończenie wszystkich procesów
    printf("%s[%s] Zarządca czeka, aż wszyscy opuszczą kompleks basenów%s\n", COLOR1, timestamp(), RESET);    
    while (wait(NULL) > 0);

    // Usunięcie kolejki komunikatów dla kasjera
    if(msgctl(ID_kolejki_kasjer, IPC_RMID, 0)==-1)
    {
        handle_error("zarzadca: msgctl ID_kolejki_kasjer");
    }

    // Usunięcie kolejki komunikatów dla ratowników - przyjmowanie klientów
    if(msgctl(ID_kolejki_ratownik_przyjmuje, IPC_RMID, 0)==-1)
    {
        handle_error("zarzadca: msgctl ID_kolejki_ratownik_przyjmuje");
    }

    // Usunięcie kolejki komunikatów dla ratowników - wypuszczanie klientów
    if(msgctl(ID_kolejki_ratownik_wypuszcza, IPC_RMID, 0)==-1)
    {
        handle_error("zarzadca: msgctl ID_kolejki_ratownik_wypuszcza");
    }

    // Usunięcie semafora dla brodzika
    if(semctl(ID_semafora_brodzik, 0, IPC_RMID)==-1)
    {
        handle_error("zarzadca: semctl ID_semafora_brodzik");
    }

    // Usunięcie semafora dla basenu rekreacyjnego
    if(semctl(ID_semafora_rekreacyjny, 0, IPC_RMID)==-1)
    {
        handle_error("zarzadca: semctl ID_semafora_rekreacyjny");
    }

    // Usunięcie semafora dla basenu olimpijskiego
    if(semctl(ID_semafora_olimpijski, 0, IPC_RMID)==-1)
    {
        handle_error("zarzadca: semctl ID_semafora_olimpijski");
    }

    // Usuwa segment pamięci dzielonej dla czas_pracy
    if(shmctl(ID_pamieci_czas_pracy, IPC_RMID, 0)==-1)
    {
        handle_error("zarzadca: shmctl ID_pamieci_czas_pracy");
    }
    if(shmdt(czas_pracy)==-1)
    {
        handle_error("zarzadca: shmdt czas_pracy");
    }

    // Usuwa segment pamięci dzielonej dla okresowe_zamkniecie
    if(shmctl(ID_pamieci_okresowe_zamkniecie, IPC_RMID, 0)==-1)
    {
        handle_error("zarzadca: shmctl ID_pamieci_okresowe_zamkniecie");
    }
    if(shmdt(okresowe_zamkniecie)==-1)
    {
        handle_error("zarzadca: shmdt okresowe_zamkniecie");
    }

    // Komunikaty o zakończeniu pracy
    printf("%s[%s] Kompleks basenów jest zamknięty%s\n", COLOR1, timestamp(), RESET);
    printf("%s[%s] Zarządca kończy działanie%s\n", COLOR1, timestamp(), RESET);

    exit(0);
}

// Wątek mierzący czas - obsługuje czas zamknięcia oraz okresowe zamknięcie
void* czasomierz()
{
    // Oblicza czas okresowego zamknięcia (połowa czasu otwarcia)
    time_t czas_otwarcia = time(NULL);
    time_t czas_polowy = czas_otwarcia + (czas_zamkniecia - czas_otwarcia) / 2;
    
    // Czas przerwy to procent całkowitego czasu otwarcia basenu
    int czas_przerwy = (int)((czas_zamkniecia - czas_otwarcia) * 0.20);
    // Jeśli czas przerwy jest krótszy niż sekunda, ustawia go na 1
    if (czas_przerwy < 1)
    {
        czas_przerwy = 1;
    }
    
    while (time(NULL) < czas_zamkniecia)
    {
        // Sprawdzenie, czy osiągnięto czas połowy otwarcia
        if (time(NULL) >= czas_polowy && !*okresowe_zamkniecie)
        {
            // Ustawienie flagi
            *okresowe_zamkniecie = true;

            // Wyświetlenie komunikatu o okresowym zamknięciu
            printf("%s[%s] Rozpoczęcie okresowego zamknięcia%s\n", COLOR1, timestamp(), RESET);

            // Wątek śpi na czas trwania przerwy
            sleep(czas_przerwy);

            // Zakończenie okresowego zamknięcia
            *okresowe_zamkniecie = false;
            printf("%s[%s] Zakończenie okresowego zamknięcia%s\n", COLOR1, timestamp(), RESET);

            // Zaktualizowanie czasu połowy, aby uniknąć kolejnej przerwy
            czas_polowy = czas_zamkniecia; // Teraz jest poza zakresem działania pętli
        }

        sleep(1);
    }

    // Osiągnięcie czasu zamknięcia
    czas_przekroczony = true;
    printf("%s[%s] Został osiągnięty czas zamknięcia%s\n", COLOR1, timestamp(), RESET);

    return NULL;
}

void *sprzatanie_klientow()
{
    while (czas_przekroczony == false)
    {
        pid_t pid_klienta;
        while(pid_klienta = waitpid(-1, NULL, WNOHANG)>0);
        sleep(1);
    }
    return NULL;
}