#include "utils.c"

// Funkcje dla wątków
void* przyjmowanie();
void* wypuszczanie();

// Zdefiniowane wyżej, aby być w stanie usunąć struktury asynchronicznie w przypadku SIGINT
int ID_kolejki_ratownik_przyjmuje;
int ID_kolejki_ratownik_wypuszcza;
int ID_semafora;
pthread_t przyjmuje, wypuszcza;

// Tablica przechowująca PID klientów aktualnie na basenie
pid_t klienci_w_basenie[MAKS_OLIMPIJSKI];
// Licznik klientów aktualnie na basenie
int licznik_klientow = 0;
// Muteks chroniący powyższe zasoby
pthread_mutex_t klient_mutex = PTHREAD_MUTEX_INITIALIZER;

// Obsługa SIGINT
void SIGINT_handler(int sig)
{
    // Anulowanie wątków przyjmowania i wypuszczania klientów
    pthread_kill(przyjmuje, SIGINT);
    pthread_kill(wypuszcza, SIGINT);

    // Wysłanie SIGINT do klientów z tablicy ratownika
    pthread_mutex_lock(&klient_mutex); // Blokowanie tablicy
    for (int i = 0; i < licznik_klientow; i++)
    {
        kill(klienci_w_basenie[i], SIGINT);
    }
    pthread_mutex_unlock(&klient_mutex); // Odblokowanie tablicy

    // Czekanie na zakończenie wszystkich procesów
    pthread_mutex_lock(&klient_mutex);
    for (int i = 0; i < licznik_klientow; i++)
    {
        int status;
        waitpid(klienci_w_basenie[i], &status, 0);
    }
    pthread_mutex_unlock(&klient_mutex);

    // Usunięcie struktur
    if(semctl(ID_semafora, 0, IPC_RMID)==-1)
    {
        handle_error("semctl ID_semafora");
    }

    // Komunikat o zakończeniu działania ratownika basenu olimpijskiego
    printf("%s[%s] Ratownik basenu olimpijskiego kończy działanie%s\n", COLOR6, timestamp(), RESET);

    exit(0);
}

void print_klienci_w_basenie()
{
    pthread_mutex_lock(&klient_mutex);
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
    pthread_mutex_unlock(&klient_mutex);
}

int main()
{
    // Obsługa SIGINT
    if (signal(SIGINT, SIGINT_handler) == SIG_ERR)
    {
        handle_error("signal SIGINT_handler");
    }
    
    // Komunikat o uruchomieniu ratownika brodzika
    printf("%s[%s] Ratownik basenu olimpijskiego uruchomiony%s\n", COLOR6, timestamp(), RESET);

    // Utworzenie kolejki do przyjmowania klientów
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

    // Utworzenie semafora
    key_t klucz_semafora = ftok(".", 9447);
    if(klucz_semafora==-1)
    {
        handle_error("ftok klucz_semafora");
    }
    ID_semafora = semget(klucz_semafora, 1, 0600|IPC_CREAT);
    if(ID_semafora==-1)
    {
        handle_error("semget ID_semafora");
    }
    if(semctl(ID_semafora, 0, SETVAL, MAKS_OLIMPIJSKI)==-1)
    {
        handle_error("semctl ID_semafora");
    }

    // Utworzenie wątków do przyjmowania i wypuszczania klientów
    pthread_create(&przyjmuje, NULL, przyjmowanie, NULL);
    pthread_create(&wypuszcza, NULL, wypuszczanie, NULL);

    pthread_join(przyjmuje, NULL);
    pthread_join(wypuszcza, NULL);

    return 0;
}

void* przyjmowanie()
{
    // Deklaracja struktur do odbierania i wysyłania wiadomości od klientów
    struct komunikat odebrany, wyslany;

    while(1)
    {
        if(msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_OLIMPIJSKI, 0)==-1)
        {
            handle_error("msgrcv ratownik olimpijski przyjmuje");
        }
        
        if(odebrany.wiek>=18)
        {
            printf("%s[%s] Ratownik olimpijski przyjął klienta %d%s\n", COLOR6, timestamp(), odebrany.PID, RESET);

            // Przyjęcie klienta na basen
            semafor_p(ID_semafora, 0);

            // Dodanie PID klienta do tablicy
            // Blokada przez muteks
            pthread_mutex_lock(&klient_mutex);
            klienci_w_basenie[licznik_klientow++] = odebrany.PID;
            pthread_mutex_unlock(&klient_mutex);

            // Ratownik zmienia wartości, aby wiadomość dotarła na PID klienta
            wyslany.mtype = odebrany.PID;
            wyslany.PID = odebrany.PID;
            wyslany.pozwolenie = true;

            // Wysłanie wiadomości
            if(msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0)==-1)
            {
                handle_error("msgsnd ID_kolejki_ratownik_przyjmuje");
            }
        }
        else
        {
            printf("%s[%s] Ratownik olimpijski nie przyjął klienta %d%s\n", COLOR6, timestamp(), odebrany.PID, RESET);

            // Ratownik zmienia wartości, aby wiadomość dotarła na PID klienta
            wyslany.mtype = odebrany.PID;
            wyslany.PID = odebrany.PID;
            wyslany.pozwolenie = false;

            // Wysłanie wiadomości
            if(msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0)==-1)
            {
                handle_error("msgsnd ID_kolejki_ratownik_przyjmuje");  
            }
        }
    }
}

void* wypuszczanie()
{
    struct komunikat odebrany, wyslany;

    while(1)
    {
        print_klienci_w_basenie();

        if(msgrcv(ID_kolejki_ratownik_wypuszcza, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_OLIMPIJSKI, 0)==-1)
        {
            handle_error("msgrcv ratownik olimpijski wypuszcza");
        }
        
        printf("%s[%s] Ratownik basenu olimpijskiego wypuścił klienta %d%s\n", COLOR6, timestamp(), odebrany.PID, RESET);

        // Usuwanie PID klienta z tablicy
        // Blokada przez muteks
        pthread_mutex_lock(&klient_mutex);

        // Znalezienie indeksu przy którym znajduje się PID klienta, który wychodzi
        int indeks_klienta_do_usuniecia;
        for (int i = 0; i < licznik_klientow; i++)
        {
            if (klienci_w_basenie[i] == odebrany.PID)
            {
                indeks_klienta_do_usuniecia = i;
                break;
            }
        }

        // Przesuniecie pozostałych PID w tablicy
        for (int i = indeks_klienta_do_usuniecia; i < licznik_klientow - 1; i++)
        {
            klienci_w_basenie[i] = klienci_w_basenie[i + 1];
        }
        licznik_klientow--;
        
        pthread_mutex_unlock(&klient_mutex);

        // Wypuszczenie klienta z basenu
        semafor_v(ID_semafora, 0);

        // Ratownik zmienia wartości, aby wiadomość dotarła na PID klienta
        wyslany.mtype = odebrany.PID;
        wyslany.PID = odebrany.PID;

        // Wysłanie wiadomości
        if(msgsnd(ID_kolejki_ratownik_wypuszcza, &wyslany, sizeof(struct komunikat) - sizeof(long), 0)==-1)
        {
            handle_error("msgsnd ID_kolejki_ratownik_wypuszcza");
        }
    }
}