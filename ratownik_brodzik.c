#include "utils.c"

// ID struktur korzystanych przez wątki
int ID_kolejki_ratownik_przyjmuje, ID_kolejki_ratownik_wypuszcza, ID_semafora_brodzik;
// Zmienna określająca czy trwa okresowe zamknięcie, pobierana z pamięci dzielonej
bool *okresowe_zamkniecie;
// Wątki
pthread_t przyjmuje, wypuszcza;
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
void SIGINT_handler(int sig);
void SIGUSR1_handler(int sig);
void SIGUSR2_handler(int sig);
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

    /*// Obsługa sygnału SIGUSR1
    if (signal(SIGUSR1, SIGUSR1_handler) == SIG_ERR)
    {
        handle_error("ratownik_brodzik: signal SIGUSR1_handler");
    }

    // Obsługa sygnału SIGUSR1
    if (signal(SIGUSR2, SIGUSR2_handler) == SIG_ERR)
    {
        handle_error("ratownik_brodzik: signal SIGUSR2_handler");
    }*/

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
    *okresowe_zamkniecie = false;

    sygnal = false;

    // Utworzenie wątków do przyjmowania i wypuszczania klientów
    if(pthread_create(&przyjmuje, NULL, przyjmowanie, NULL) != 0)
    {
        handle_error("ratownik_brodzik: pthread_create przyjmuje");
    }
    if(pthread_create(&wypuszcza, NULL, wypuszczanie, NULL) != 0)
    {
        handle_error("ratownik_brodzik: pthread_create wypuszcza");
    }

    // Dołączenie wątków do przyjmowania i wypuszczania klientów
    if(pthread_join(przyjmuje, NULL) != 0)
    {
        handle_error("ratownik_brodzik: pthread_join przyjmuje");
    }
    if(pthread_join(wypuszcza, NULL) != 0)
    {
        handle_error("ratownik_brodzik: pthread_join wypuszczanie");
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
        // Odebranie wiadomości
        if(msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_BRODZIK, 0) == -1)
        {
            handle_error("ratownik_brodzik: msgrcv ID_kolejki_ratownik_przyjmuje");
        }

        // Decyzja o przyjęciu klienta
        if(odebrany.wiek <= 5 && *okresowe_zamkniecie == false && sygnal == false) // Sprawdzenie, czy klient ma odpowiedni wiek, nie trwa okresowe zamknięcie lub nie został wysłany sygnał
        {
            semafor_p(ID_semafora_brodzik, 0); // Obniżenie semafora - jeśli nie ma aktualnie miejsca na basenie, czeka
            wyslany.pozwolenie = true; // Klient dostaje pozwolenie
            pthread_mutex_lock(&klient_mutex); // Blokada muteksu
            if(licznik_klientow>=MAKS_BRODZIK)
            {
                handle_error("ratownik_brodzik: licznik_klientow poza zakresem");
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