#include "utils.c"

// ID struktur korzystanych przez wątki
int ID_kolejki_ratownik_przyjmuje, ID_kolejki_ratownik_wypuszcza, ID_semafora_olimpijski;
// Zmienna określająca czy trwa okresowe zamknięcie, pobierana z pamięci dzielonej
bool *okresowe_zamkniecie;
// Wątki
pthread_t przyjmuje, wypuszcza;
// Flaga sprawdzające, czy wydarzenie zostało już obsłużone
bool zamkniecie_handled = false;
// Zmienna z informacją, czy został wysłany sygnał SIGUSR1
bool sygnal;
// Tablica przechowująca PID klientów aktualnie na basenie
pid_t klienci_w_basenie[MAKS_OLIMPIJSKI];
// Licznik klientów aktualnie na basenie
int licznik_klientow = 0;
// Muteks chroniący powyższe zasoby
pthread_mutex_t klient_mutex = PTHREAD_MUTEX_INITIALIZER;

void* przyjmowanie();
void* wypuszczanie();
void okresowe_zamkniecie_handler();
void SIGINT_handler(int sig);
void SIGUSR1_handler(int sig);
void SIGUSR2_handler(int sig);
void wyswietl_basen();

int main()
{
    // Komunikat o uruchomieniu ratownika brodzika
    printf("%s[%s] Ratownik basenu olimpijskiego uruchomiony%s\n", COLOR6, timestamp(), RESET);

    // Obsługa SIGINT
    if (signal(SIGINT, SIGINT_handler) == SIG_ERR)
    {
        handle_error("ratownik_olimpijski: signal SIGINT_handler");
    }

    /*// Obsługa sygnału SIGUSR1
    if (signal(SIGUSR1, SIGUSR1_handler) == SIG_ERR)
    {
        handle_error("ratownik_olimpijski: signal SIGUSR1_handler");
    }

    // Obsługa sygnału SIGUSR1
    if (signal(SIGUSR2, SIGUSR2_handler) == SIG_ERR)
    {
        handle_error("ratownik_olimpijski: signal SIGUSR2_handler");
    }*/
    
    // Uzyskanie dostępu do kolejki komunikatów dla ratowników - przyjmowanie klientów
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    if(klucz_kolejki_ratownik_przyjmuje==-1)
    {
        handle_error("ratownik_rekreacyjny: ftok klucz_kolejki_ratownik_przyjmuje");
    }
    ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_przyjmuje==-1)
    {
        handle_error("ratownik_rekreacyjny: msgget ID_kolejki_ratownik_przyjmuje");
    }

    // Uzyskanie dostępu do kolejki komunikatów dla ratowników - wypuszczanie klientów
    key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
    if(klucz_kolejki_ratownik_wypuszcza==-1)
    {
        handle_error("ratownik_rekreacyjny: ftok klucz_kolejki_ratownik_wypuszcza");
    }
    ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_wypuszcza==-1)
    {
        handle_error("ratownik_rekreacyjny: msgget ID_kolejki_ratownik_wypuszcza");
    }

    // Utworzenie semafora dla basenu olimpijskiego
    key_t klucz_semafora_olimpijski = ftok(".", 9447);
    if(klucz_semafora_olimpijski==-1)
    {
        handle_error("ratownik_rekreacyjny: ftok klucz_semafora_olimpijski");
    }
    ID_semafora_olimpijski = semget(klucz_semafora_olimpijski, 1, 0600|IPC_CREAT);
    if(ID_semafora_olimpijski==-1)
    {
        handle_error("ratownik_rekreacyjny: semget ID_semafora_olimpijski");
    }
    if(semctl(ID_semafora_olimpijski, 0, SETVAL, MAKS_OLIMPIJSKI)==-1)
    {
        handle_error("ratownik_rekreacyjny: semctl ID_semafora_olimpijski");
    }

    // Uzyskanie dostępu do segmentu pamięci dzielonej, która przechowuje zmienną bool okresowe_zamkniecie
    key_t klucz_pamieci_okresowe_zamkniecie = ftok(".", 9929);
    if(klucz_pamieci_okresowe_zamkniecie==-1)
    {
        handle_error("ratownik_rekreacyjny: ftok klucz_pamieci_okresowe_zamkniecie");
    }
    int ID_pamieci_okresowe_zamkniecie = shmget(klucz_pamieci_okresowe_zamkniecie, sizeof(bool), 0600 | IPC_CREAT);
    if(ID_pamieci_okresowe_zamkniecie==-1)
    {
        handle_error("ratownik_rekreacyjny: shmget ID_pamieci_okresowe_zamkniecie");
    }
    okresowe_zamkniecie = (bool*)shmat(ID_pamieci_okresowe_zamkniecie, NULL, 0);
    if (okresowe_zamkniecie == (void*)-1)
    {
        handle_error("ratownik_rekreacyjny: shmat okresowe_zamkniecie");
    }

    sygnal = false;

    // Utworzenie wątków do przyjmowania i wypuszczania klientów
    if(pthread_create(&przyjmuje, NULL, przyjmowanie, NULL) != 0)
    {
        handle_error("ratownik_rekreacyjny: pthread_create przyjmuje");
    }
    if(pthread_create(&wypuszcza, NULL, wypuszczanie, NULL) != 0)
    {
        handle_error("ratownik_rekreacyjny: pthread_create wypuszcza");
    }

    // Dołączenie wątków do przyjmowania i wypuszczania klientów
    if(pthread_join(przyjmuje, NULL) != 0)
    {
        handle_error("ratownik_rekreacyjny: pthread_join przyjmuje");
    }
    if(pthread_join(wypuszcza, NULL) != 0)
    {
        handle_error("ratownik_rekreacyjny: pthread_join wypuszczanie");
    }

    return 0;
}

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
        }

        // Odebranie wiadomości
        if(msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_OLIMPIJSKI, 0)==-1)
        {
            handle_error("ratownik_olimpijski: msgrcv ID_kolejki_ratownik_przyjmuje");
        }
        
        // Decyzja o przyjęciu klienta
        if(odebrany.wiek>=18 && *okresowe_zamkniecie == false && sygnal == false) // Sprawdzenie, czy klient ma odpowiedni wiek, nie trwa okresowe zamknięcie lub nie został wysłany sygnał
        {
            semafor_p(ID_semafora_olimpijski, 0); // Obniżenie semafora - jeśli nie ma aktualnie miejsca na basenie, czeka
            wyslany.pozwolenie = true; // Klient dostaje pozwolenie
            pthread_mutex_lock(&klient_mutex); // Blokada muteksu
            if(licznik_klientow>=MAKS_OLIMPIJSKI)
            {
                handle_error("ratownik_rekreacyjny: licznik_klientow poza zakresem");
            }
            klienci_w_basenie[licznik_klientow++] = odebrany.PID; // Dodanie klienta do tablicy i podwyższenie licznika klientów
            pthread_mutex_unlock(&klient_mutex); // Odblokowanie muteksu
        }
        else
        {
            wyslany.pozwolenie = false;
        }

        // Wysłanie wiadomości
        wyslany.mtype = odebrany.PID;
        if(msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0)==-1)
        {
            handle_error("ratownik_rekreacyjny: msgsnd ID_kolejki_ratownik_przyjmuje");
        }

        // Komunikat o przyjęciu lub nie klienta
        if(wyslany.pozwolenie == true)
        {
            printf("%s[%s] Ratownik olimpijski przyjął klienta %d%s\n", COLOR6, timestamp(), odebrany.PID, RESET);
        }
        else
        {
            printf("%s[%s] Ratownik olimpijski nie przyjął klienta %d%s\n", COLOR6, timestamp(), odebrany.PID, RESET);
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
        if(msgrcv(ID_kolejki_ratownik_wypuszcza, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_OLIMPIJSKI, 0)==-1)
        {
            handle_error("ratownik_olimpijski: msgrcv ID_kolejki_ratownik_wypuszcza");
        }
        
        semafor_v(ID_semafora_olimpijski, 0);

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
        if(msgsnd(ID_kolejki_ratownik_wypuszcza, &wyslany, sizeof(struct komunikat) - sizeof(long), 0)==-1)
        {
            handle_error("msgsnd ID_kolejki_ratownik_wypuszcza");
        }

        // Komunikat o wypuszczeniu klienta
        printf("%s[%s] Ratownik basenu olimpijskiego wypuścił klienta %d%s\n", COLOR6, timestamp(), odebrany.PID, RESET);

        // Wyświetlenie aktualnego stanu basenu
        wyswietl_basen();
    }
}

// Obsługa okresowego zamknięcia
void okresowe_zamkniecie_handler()
{
    pthread_mutex_lock(&klient_mutex); // Blokada muteksu
    if (!zamkniecie_handled) // Sprawdza, czy okresowe zamknięcie nie zostało już obsłużone
    {
        for (int i = 0; i < licznik_klientow; i++) // Usuwa wszystkie PID w tablicy
        {
            klienci_w_basenie[i] = 0;
        }
        licznik_klientow = 0; // Resetuje licznik klientów
        semctl(ID_semafora_olimpijski, 0, SETVAL, MAKS_OLIMPIJSKI); // Ustawia wartość semafora tak jakby nikogo nie było w basenie
        zamkniecie_handled = true; // Oznacza okresowe zamknięcie jako obsłużone
        printf("%s[%s] Ratownik basenu olimpijskiego wyprosił wszystkich klientów%s\n", COLOR6, timestamp(), RESET);
    }
    pthread_mutex_unlock(&klient_mutex);
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
        handle_error("ratownik_olimpijski: pthread_mutex_destroy klient_mutex");
    }

    // Komunikat o zakończeniu działania ratownika basenu olimpijskiego
    printf("%s[%s] Ratownik basenu olimpijskiego kończy działanie%s\n", COLOR6, timestamp(), RESET);

    exit(0);
}

void wyswietl_basen()
{
    printf("%s[%s] Basen olimpijski: [", COLOR6, timestamp());
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