#include "utils.c"

// ID struktur korzystanych przez wątki
int ID_kolejki_ratownik_przyjmuje, ID_kolejki_ratownik_wypuszcza, ID_semafora_brodzik;
// Zmienne z pamięci dzielonej
int *czas_pracy;
bool *okresowe_zamkniecie;
// Wątki
pthread_t przyjmuje, wypuszcza, wysyla_sygnal;
// Flaga sprawdzające, czy wydarzenie zostało już obsłużone
bool zamkniecie_handled = false;
// Zmienna z informacją, czy został wysłany sygnał SIGUSR1
bool sygnal;
// Tablica przechowująca PID klientów aktualnie na basenie
pid_t klienci_w_basenie[MAKS_BRODZIK];
// Licznik klientów aktualnie na basenie
int licznik_klientow = 0;
// Muteks chroniący powyższe zasoby
pthread_mutex_t klient_mutex = PTHREAD_MUTEX_INITIALIZER;

void* przyjmowanie();
void* wypuszczanie();
void *wysylanie_sygnalow();
void okresowe_zamkniecie_handler();
void SIGINT_handler(int sig);
void wyswietl_basen();

int main()
{
    // Komunikat o uruchomieniu ratownika brodzika
    printf("%s[%s] Ratownik brodzika uruchomiony%s\n", COLOR4, timestamp(), RESET);

    // Obsługa SIGINT
    if(signal(SIGINT, SIGINT_handler) == SIG_ERR)
    {
        handle_error("ratownik_brodzik: signal SIGINT_handler");
    }

    // Uzyskanie dostępu do kolejki komunikatów dla ratowników - przyjmowanie klientów
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    if(klucz_kolejki_ratownik_przyjmuje==-1)
    {
        handle_error("ratownik_brodzik: ftok klucz_kolejki_ratownik_przyjmuje");
    }
    ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_przyjmuje==-1)
    {
        handle_error("ratownik_brodzik: msgget ID_kolejki_ratownik_przyjmuje");
    }

    // Uzyskanie dostępu do kolejki komunikatów dla ratowników - wypuszczanie klientów
    key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
    if(klucz_kolejki_ratownik_wypuszcza==-1)
    {
        handle_error("ratownik_brodzik: ftok klucz_kolejki_ratownik_wypuszcza");
    }
    ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_wypuszcza==-1)
    {
        handle_error("ratownik_brodzik: msgget ID_kolejki_ratownik_wypuszcza");
    }

    // Uzyskanie dostępu do semafora dla brodzika
    key_t klucz_semafora_brodzik = ftok(".", 3293);
    if(klucz_semafora_brodzik == -1)
    {
        handle_error("ratownik_brodzik: ftok klucz_semafora_brodzik");
    }
    ID_semafora_brodzik = semget(klucz_semafora_brodzik, 1, 0600 | IPC_CREAT);
    if(ID_semafora_brodzik == -1)
    {
        handle_error("ratownik_brodzik: semget ID_semafora_brodzik");
    }

    // Uzyskanie dostępu do segmentu pamięci dzielonej, która przechowuje zmienną czas_pracy
    key_t klucz_pamieci_czas_pracy = ftok(".", 1400);
    if(klucz_pamieci_czas_pracy==-1)
    {
        handle_error("ratownik_brodzik: ftok klucz_pamieci_czas_pracy");
    }
    int ID_pamieci_czas_pracy = shmget(klucz_pamieci_czas_pracy, sizeof(int), 0600 | IPC_CREAT);
    if(ID_pamieci_czas_pracy==-1)
    {
        handle_error("ratownik_brodzik: shmget ID_pamieci_czas_pracy");
    }
    czas_pracy = (int*)shmat(ID_pamieci_czas_pracy, NULL, 0);
    if (czas_pracy == (void*)-1)
    {
        handle_error("ratownik_brodzik: shmat czas_pracy");
    }

    // Uzyskanie dostępu do segmentu pamięci dzielonej, która przechowuje zmienną bool okresowe_zamkniecie
    key_t klucz_pamieci_okresowe_zamkniecie = ftok(".", 9929);
    if(klucz_pamieci_okresowe_zamkniecie==-1)
    {
        handle_error("ratownik_brodzik: ftok klucz_pamieci_okresowe_zamkniecie");
    }
    int ID_pamieci_okresowe_zamkniecie = shmget(klucz_pamieci_okresowe_zamkniecie, sizeof(bool), 0600 | IPC_CREAT);
    if(ID_pamieci_okresowe_zamkniecie==-1)
    {
        handle_error("ratownik_brodzik: shmget ID_pamieci_okresowe_zamkniecie");
    }
    okresowe_zamkniecie = (bool*)shmat(ID_pamieci_okresowe_zamkniecie, NULL, 0);
    if (okresowe_zamkniecie == (void*)-1)
    {
        handle_error("ratownik_brodzik: shmat okresowe_zamkniecie");
    }

    sygnal = false;

    // Utworzenie wątków do przyjmowania, wypuszczania klientów i wysyłania sygnałów
    if(pthread_create(&przyjmuje, NULL, przyjmowanie, NULL) != 0)
    {
        handle_error("ratownik_brodzik: pthread_create przyjmuje");
    }
    if(pthread_create(&wypuszcza, NULL, wypuszczanie, NULL) != 0)
    {
        handle_error("ratownik_brodzik: pthread_create wypuszcza");
    }
    if(pthread_create(&wysyla_sygnal, NULL, wysylanie_sygnalow, NULL) != 0)
    {
        handle_error("ratownik_brodzik: pthread_create wysyla_sygnal");
    }

    // Dołączenie wątków do przyjmowania, wypuszczania klientów i wysyłania sygnałów
    if(pthread_join(przyjmuje, NULL) != 0)
    {
        handle_error("ratownik_brodzik: pthread_join przyjmuje");
    }
    if(pthread_join(wypuszcza, NULL) != 0)
    {
        handle_error("ratownik_brodzik: pthread_join wypuszcza");
    }
    if(pthread_join(wysyla_sygnal, NULL) != 0)
    {
        handle_error("ratownik_brodzik: pthread_join wysyla_sygnal");
    }

    return 0;
}

// Wątek przyjmujący klientów do brodzika
void* przyjmowanie()
{  
    // Deklaracja struktur do odbierania i wysyłania wiadomości od klientów
    struct komunikat odebrany, wyslany;

    while(1)
    {
        // Jeśli trwa okresowe zamknięcie, wywołuje obsługującą funkcję
        if(*okresowe_zamkniecie)
        {
            okresowe_zamkniecie_handler();
            sleep(1);
            continue;
        }

        // Odebranie wiadomości
        if(msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_BRODZIK, 0) == -1)
        {
            handle_error("ratownik_brodzik: msgrcv ID_kolejki_ratownik_przyjmuje");
        }

        wyslany.pozwolenie = false; // Zakłada że klient nie zostanie przyjęty. Jeśli go przyjmie, zmienia na true, jeśli nie, zostaje false

        // Decyzja o przyjęciu klienta
        if(odebrany.wiek <= 5 && *okresowe_zamkniecie == false && sygnal == false) // Sprawdzenie, czy klient ma odpowiedni wiek, nie trwa okresowe zamknięcie lub nie został wysłany sygnał
        {
            if(*okresowe_zamkniecie==true)
            {
                continue;
            }
            semafor_p(ID_semafora_brodzik, 0); // Obniżenie semafora - jeśli nie ma aktualnie miejsca na basenie, czeka
            if(sygnal == false)
            {
                wyslany.pozwolenie = true; // Klient dostaje pozwolenie
                pthread_mutex_lock(&klient_mutex); // Blokada muteksu
                if(licznik_klientow>=MAKS_BRODZIK)
                {
                    handle_error("ratownik_brodzik: licznik_klientow poza zakresem");
                }
                klienci_w_basenie[licznik_klientow++] = odebrany.PID; // Dodanie klienta do tablicy i podwyższenie licznika klientów
                pthread_mutex_unlock(&klient_mutex); // Odblokowanie muteksu
            }
        }

        // Wysłanie wiadomości
        wyslany.mtype = odebrany.PID;
        if(msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
        {
            handle_error("ratownik_brodzik: msgsnd ID_kolejki_ratownik_przyjmuje");
        }

        // Komunikat o przyjęciu lub nie klienta
        if(wyslany.pozwolenie == true)
        {
            printf("%s[%s] Ratownik brodzika przyjął klienta %d z opiekunem%s\n", COLOR4, timestamp(), odebrany.PID, RESET);
        }
        else
        {
            printf("%s[%s] Ratownik brodzika nie przyjął klienta %d%s\n", COLOR4, timestamp(), odebrany.PID, RESET);
        }

        // Wyświetlenie aktualnego stanu basenu
        wyswietl_basen();
    }
}

// Wątek wypuszczający klientów z brodzika
void* wypuszczanie()
{
    // Deklaracja struktur wysyłanych i odbieranych od klienta
    struct komunikat odebrany, wyslany;

    while(1) // Dopóki ratownik nie dostanie SIGINT od zarządcy
    {
        // Odebranie wiadomości
        if(msgrcv(ID_kolejki_ratownik_wypuszcza, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_BRODZIK, 0) == -1)
        {
            handle_error("ratownik_brodzik: msgrcv ID_kolejki_ratownik_wypuszcza");
        }

        semafor_v(ID_semafora_brodzik, 0);

        // Usunięcie klienta wychodzącego z tablicy
        pthread_mutex_lock(&klient_mutex); // Blokada muteksu
        int indeks_klienta_do_usuniecia;
        for (int i = 0; i < licznik_klientow; i++) // Znaleznie indeksu pod którym znajduje się PID usuwanego klienta
        {
            if (klienci_w_basenie[i] == odebrany.PID)
            {
                indeks_klienta_do_usuniecia = i;
                break;
            }
        }
        for (int i = indeks_klienta_do_usuniecia; i < licznik_klientow - 1; i++) // Przesunięcie pozostałych klientów w tablicy
        {
            klienci_w_basenie[i] = klienci_w_basenie[i + 1];
        }
        licznik_klientow--; // Obniżenie licznika klientów
        pthread_mutex_unlock(&klient_mutex); // Odblokowanie muteksu

        // Wysłanie wiadomości
        wyslany.mtype = odebrany.PID;
        if(msgsnd(ID_kolejki_ratownik_wypuszcza, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
        {
            handle_error("ratownik_brodzik: msgsnd ID_kolejki_ratownik_wypuszcza");
        }

        // Komunikat o wypuszczeniu klienta
        printf("%s[%s] Ratownik brodzika wypuścił klienta %d z opiekunem%s\n", COLOR4, timestamp(), odebrany.PID, RESET);
        
        // Wyświetlenie aktualnego stanu basenu
        wyswietl_basen();
    }
}

// Funkcja dla wątku wysyłającego sygnały SIGUSR1 i SIGUSR2
void *wysylanie_sygnalow()
{
    srand(getpid());
    // Wylosowanie godzin wysłania sygnałów - mają się zmieścić w czas otwarcia kompleksu
    time_t start = time(NULL); // Zmienna z początkiem pracy wątku (ratownika)
    time_t wyslij_SIGUSR1 = start + rand() % (*czas_pracy / 4) + 10;
    time_t wyslij_SIGUSR2 = wyslij_SIGUSR1 + rand() % (*czas_pracy / 4) + 5;

    // SIGUSR1
    // Oczekiwanie na czas wysłania sygnału
    while (time(NULL) < wyslij_SIGUSR1)
    {
        sleep(1);
    }

    // Ustawia zmienną globalną sygnał na true - ratownik nie przyjmuje klientów
    sygnal = true;

    // Wysyła sygnał do klientów w tablicy
    pid_t pid_SIGUSR1[MAKS_BRODZIK]; // Tablica do zapamiętywania, którzy klienci otrzymali SIGUSR1, aby potem im odesłać SIGUSR2
    int licznik_SIGUSR1 = 0;
    for (int i = 0; i < licznik_klientow; i++)
    {
        kill(klienci_w_basenie[i], SIGUSR1);
        pid_SIGUSR1[licznik_SIGUSR1++] = klienci_w_basenie[i];   
    }

    // Komunikat o sygnale
    printf("%s[%s] Ratownik brodzika wywołał SIGUSR1%s\n", COLOR4, timestamp(), RESET);

    // Wyprasza klientów - usuwa PID z tablicy i resetuje licznik klientów
    pthread_mutex_lock(&klient_mutex); // Blokada muteksu
    if (!zamkniecie_handled) // Sprawdza, czy okresowe zamknięcie nie zostało już obsłużone
    {
        for (int i = 0; i < licznik_klientow; i++) // Usuwa wszystkie PID w tablicy
        {
            klienci_w_basenie[i] = 0;
        }
        licznik_klientow = 0; // Resetuje licznik klientów
        if(semctl(ID_semafora_brodzik, 0, SETVAL, MAKS_BRODZIK) == -1) // Ustawia wartość semafora tak jakby nikogo nie było w basenie
        {
            handle_error("ratownik_brodzik: semctl SETVAL ID_semafora_brodzik");
        }
        printf("%s[%s] Ratownik brodzika wyprosił wszystkich klientów%s\n", COLOR4, timestamp(), RESET);
    }
    pthread_mutex_unlock(&klient_mutex);

    // Wyświetlenie aktualnego stanu basenu
    wyswietl_basen();

    // SIGUSR2
    // Oczekiwanie na czas wysłania sygnału
    while (time(NULL) < wyslij_SIGUSR2)
    {
        sleep(1);
    }

    // Ustawia zmienną globalną sygnał na true - ratownik przyjmuje klientów
    sygnal = false;

    // Wysyła sygnał do klientów w tablicy
    for (int i = 0; i < licznik_klientow; i++)
    {
        kill(klienci_w_basenie[i], SIGUSR2);
    }

    // Komunikat o sygnale
    printf("%s[%s] Ratownik brodzika wywołał SIGUSR2%s\n", COLOR4, timestamp(), RESET);

    // Zakończenie wątku 
    pthread_exit(NULL);
}

// Obsługa okresowego zamknięcia
void okresowe_zamkniecie_handler()
{
    if (!zamkniecie_handled) // Sprawdza, czy okresowe zamknięcie nie zostało już obsłużone
    {
        pthread_mutex_lock(&klient_mutex); // Blokada muteksu
        for (int i = 0; i < licznik_klientow; i++) // Usuwa wszystkie PID w tablicy
        {
            klienci_w_basenie[i] = 0;
        }
        licznik_klientow = 0; // Resetuje licznik klientów
        pthread_mutex_unlock(&klient_mutex);
        if(semctl(ID_semafora_brodzik, 0, SETVAL, MAKS_BRODZIK) == -1) // Ustawia wartość semafora tak jakby nikogo nie było w basenie
        {
            handle_error("ratownik_brodzik: semctl SETVAL ID_semafora_brodzik");
        }
        zamkniecie_handled = true; // Oznacza okresowe zamknięcie jako obsłużone
        printf("%s[%s] Ratownik brodzika wyprosił wszystkich klientów%s\n", COLOR4, timestamp(), RESET);
        wyswietl_basen();
    }
}

// Obsługa SIGINT
void SIGINT_handler(int sig)
{
    // Anulowanie wątków przyjmowania i wypuszczania klientów
    pthread_cancel(przyjmuje);
    pthread_cancel(wypuszcza);

    // Usunięcie muteksu
    if (pthread_mutex_destroy(&klient_mutex) != 0)
    {
        handle_error("ratownik_brodzik: pthread_mutex_destroy klient_mutex");
    }

    // Komunikat o zakończeniu działania ratownika brodzika
    printf("%s[%s] Ratownik brodzika kończy działanie%s\n", COLOR4, timestamp(), RESET);

    exit(0);
}

void wyswietl_basen()
{
    printf("%s[%s] Brodzik: [", COLOR4, timestamp());
    for (int i = 0; i < licznik_klientow; i++)
    {
        printf("%d", klienci_w_basenie[i]);
        if (i < licznik_klientow - 1)
        {
            printf(", ");
        }
    }
    printf("]%s\n", RESET);
}