#include "utils.c"

// Definicje globalne
pid_t PID_kasjera, PID_ratownika_brodzik, PID_ratownika_rekreacyjny, PID_ratownika_olimpijski; // Identyfikatory struktur
int ID_kolejki_ratownik_przyjmuje, ID_kolejki_ratownik_wypuszcza, ID_pamieci_czas_przekroczony, ID_pamieci_okresowe_zamkniecie, ID_semafora_brodzik, ID_semafora_rekreacyjny, ID_semafora_olimpijski;
time_t czas_zamkniecia; // Zmienne dotyczące czasu korzystane przez funkcję main i wątek
bool* czas_przekroczony;
bool* okresowe_zamkniecie; // Flagi w pamięci dzielonej, korzystane przez inne procesy

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

    // Usuwa segment pamięci dzielonej dla czas_przekroczony
    if(shmctl(ID_pamieci_czas_przekroczony, IPC_RMID, 0)==-1)
    {
        handle_error("shmctl ID_pamieci_czas_przekroczony");
    }
    if(shmdt(czas_przekroczony)==-1)
    {
        handle_error("shmdt czas_przekroczony");
    }

    // Usuwa segment pamięci dzielonej dla okresowe_zamkniecie
    if(shmctl(ID_pamieci_okresowe_zamkniecie, IPC_RMID, 0)==-1)
    {
        handle_error("shmctl ID_pamieci_okresowe_zamkniecie");
    }
    if(shmdt(okresowe_zamkniecie)==-1)
    {
        handle_error("shmdt okresowe_zamkniecie");
    }

    // Usuwa kolejki komunikatów ratowników
    if(msgctl(ID_kolejki_ratownik_przyjmuje, IPC_RMID, 0)==-1)
    {
        handle_error("msgctl ID_kolejki_ratownik_przyjmuje");
    }
    if(msgctl(ID_kolejki_ratownik_wypuszcza, IPC_RMID, 0)==-1)
    {
        handle_error("msgctl ID_kolejki_ratownik_wypuszcza");
    }

    // Usunięcie semaforów ratowników
    if(semctl(ID_semafora_brodzik, 0, IPC_RMID)==-1)
    {
        handle_error("semctl ID_semafora");
    }
    if(semctl(ID_semafora_rekreacyjny, 0, IPC_RMID)==-1)
    {
        handle_error("semctl ID_semafora");
    }
    if(semctl(ID_semafora_olimpijski, 0, IPC_RMID)==-1)
    {
        handle_error("semctl ID_semafora");
    }

    // Zarządca czeka, aż nadejdzie czas zamknięcia (jeśli czas został już przekroczony stanie się to od razu)
    /*if(pthread_join(czyszczenie, NULL) == -1)
    {
        handle_error("pthread_join czyszczenie");
    }*/

    // Komunikaty o zakończeniu pracy
    printf("%s[%s] Kompleks basenów jest zamknięty%s\n", COLOR1, timestamp(), RESET);
    printf("%s[%s] Zarządca kończy działanie%s\n", COLOR1, timestamp(), RESET);

    exit(0);
}

/*void* czyszczenie*()
{
    while (wait(NULL) > 0);
}*/

// Wątek mierzący czas - obsługuje czas zamknięcia oraz okresowe zamknięcie
void* czasomierz()
{
    // Oblicza czas okresowego zamknięcia (połowa czasu otwarcia)
    time_t czas_otwarcia = time(NULL);
    time_t czas_polowy = czas_otwarcia + (czas_zamkniecia - czas_otwarcia) / 2;
    
    // Czas przerwy to CZAS_PRZERWY_PROCENT całkowitego czasu otwarcia basenu
    int czas_przerwy = (int)((czas_zamkniecia - czas_otwarcia) * CZAS_PRZERWY_PROCENT);
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
    *czas_przekroczony = true;
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
    // Zczytanie wartości wykonania powyższego polecenia z pliku fp
    int maks_procesy = 0;
    if(fscanf(fp, "%d", &maks_procesy) != 1)
    {
        pclose(fp);
        printf("scanf maks_procesy\n");
        exit(EXIT_FAILURE);
    }
    pclose(fp);

    // Ustalenie maksymalnej liczby klientów
    int maks_klientow = 0;
    printf("Podaj maksymalną liczbę klientów: ");
    if(scanf("%d", &maks_klientow) != 1) // Użytkownik podaje maksymalną liczbę klientów
    {
        printf("scanf maks_klientow - podano złą wartość\n");
        exit(EXIT_FAILURE);
    }
    // Sprawdzenie czy liczba wywołanych procesów jest poprawna:
    // Nie mniejsza od 0
    if(maks_klientow<0)
    {
        printf("maks_klientow - podano za małą wartość\n");
        exit(EXIT_FAILURE);
    }
    // Nie większa niż limit
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
    if(czas_zamkniecia<time(NULL) || czas_pracy < 0)
    {
        printf("czas_zamkniecia - podano za krótki czas pracy\n");
        exit(EXIT_FAILURE);
    }

    // Komunikat o otwarciu kompleksu basenów
    printf("%s[%s] Kompleks basenów jest otwarty%s\n", COLOR1, timestamp(), RESET);

    // Utworzenie segmentu pamięci dzielonej, która przechowuje zmienną bool czas_przekroczony
    key_t klucz_pamieci_czas_przekroczony = ftok(".", 3213);
    if(klucz_pamieci_czas_przekroczony==-1)
    {
        handle_error("ftok klucz_pamieci_czas_przekroczony");
    }
    ID_pamieci_czas_przekroczony = shmget(klucz_pamieci_czas_przekroczony, sizeof(bool), 0600 | IPC_CREAT);
    if(ID_pamieci_czas_przekroczony==-1)
    {
        handle_error("shmget ID_pamieci_czas_przekroczony");
    }
    czas_przekroczony = (bool*)shmat(ID_pamieci_czas_przekroczony, NULL, 0);
    if (czas_przekroczony == (void*)-1)
    {
        handle_error("shmat czas_przekroczony");
    }
    // Inicjalizacja zmiennej jako "false" - czas nie został przekroczony
    *czas_przekroczony = false;

    // Utworzenie segmentu pamięci dzielonej, która przechowuje zmienną bool okresowe_zamkniecie
    key_t klucz_pamieci_okresowe_zamkniecie = ftok(".", 9929);
    if(klucz_pamieci_okresowe_zamkniecie==-1)
    {
        handle_error("ftok klucz_pamieci_okresowe_zamkniecie");
    }
    ID_pamieci_okresowe_zamkniecie = shmget(klucz_pamieci_okresowe_zamkniecie, sizeof(bool), 0600 | IPC_CREAT);
    if(ID_pamieci_okresowe_zamkniecie==-1)
    {
        handle_error("shmget ID_pamieci_okresowe_zamkniecie");
    }
    okresowe_zamkniecie = (bool*)shmat(ID_pamieci_okresowe_zamkniecie, NULL, 0);
    if (okresowe_zamkniecie == (void*)-1)
    {
        handle_error("shmat okresowe_zamkniecie");
    }
    // Inicjalizacja zmiennej jako "false" - nie ma aktualnie okresowego zamkniecia
    *okresowe_zamkniecie = false;

    // Tworzymy wątek, który monitoruje czas zamknięcia
    pthread_t czas;
    if(pthread_create(&czas, NULL, czasomierz, NULL)!=0)
    {
        handle_error("pthread_create czas");
    }

    // Utworzenie kolejek komunikatów dla ratowników, do przyjmowania i wypuszczania klientów
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

        // Utworzenie semafora
    key_t klucz_semafora_brodzik = ftok(".", 3293);
    if(klucz_semafora_brodzik == -1)
    {
        handle_error("ftok klucz_semafora");
    }
    ID_semafora_brodzik = semget(klucz_semafora_brodzik, 1, 0600 | IPC_CREAT);
    if(ID_semafora_brodzik == -1)
    {
        handle_error("semget ID_semafora");
    }
    if(semctl(ID_semafora_brodzik, 0, SETVAL, MAKS_BRODZIK) == -1)
    {
        handle_error("semctl SETVAL");
    }
  

 // Utworzenie semafora
    key_t klucz_semafora_rekreacyjny = ftok(".", 2003);
    if(klucz_semafora_rekreacyjny == -1)
    {
        handle_error("ftok klucz_semafora");
    }
    ID_semafora_rekreacyjny = semget(klucz_semafora_rekreacyjny, 1, 0600 | IPC_CREAT);
    if(ID_semafora_rekreacyjny == -1)
    {
        handle_error("semget ID_semafora");
    }
    if(semctl(ID_semafora_rekreacyjny, 0, SETVAL, MAKS_REKREACYJNY) == -1)
    {
        handle_error("semctl SETVAL");
    }

    // Utworzenie semafora
    key_t klucz_semafora_olimpijski = ftok(".", 9447);
    if(klucz_semafora_olimpijski==-1)
    {
        handle_error("ftok klucz_semafora");
    }
    ID_semafora_olimpijski = semget(klucz_semafora_olimpijski, 1, 0600|IPC_CREAT);
    if(ID_semafora_olimpijski==-1)
    {
        handle_error("semget ID_semafora");
    }
    if(semctl(ID_semafora_olimpijski, 0, SETVAL, MAKS_OLIMPIJSKI)==-1)
    {
        handle_error("semctl ID_semafora");
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