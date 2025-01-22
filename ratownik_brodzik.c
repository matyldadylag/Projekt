#include "utils.c"

// Funkcje dla wątków
void* przyjmowanie();
void* wypuszczanie();

// Zdefiniowane wyżej, aby być w stanie usunąć struktury asynchronicznie w przypadku SIGINT
int ID_kolejki_ratownik_przyjmuje;
int ID_kolejki_ratownik_wypuszcza;
pthread_t przyjmuje, wypuszcza;
int ID_semafora_brodzik;

// Tablica przechowująca PID klientów aktualnie na basenie
pid_t klienci_w_basenie[MAKS_BRODZIK];
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

    pthread_mutex_lock(&klient_mutex); // Blokowanie tablicy
    for (int i = 0; i < licznik_klientow; i++)
    {
        kill(klienci_w_basenie[i], SIGINT);
    }

    // Komunikat o zakończeniu działania ratownika brodzika
    printf("%s[%s] Ratownik brodzika kończy działanie%s\n", COLOR4, timestamp(), RESET);

    exit(0);
}

void print_klienci_w_basenie()
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

int main()
{
    // Obsługa SIGINT
    if (signal(SIGINT, SIGINT_handler) == SIG_ERR)
    {
        handle_error("signal SIGINT_handler");
    }
  
    // Komunikat o uruchomieniu ratownika brodzika
    printf("%s[%s] Ratownik brodzika uruchomiony%s\n", COLOR4, timestamp(), RESET);

    // Utworzenie kolejki do przyjmowania klientów
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    if(klucz_kolejki_ratownik_przyjmuje == -1)
    {
        handle_error("ftok klucz_kolejki_ratownik_przyjmuje");
    }
    ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_przyjmuje == -1)
    {
        handle_error("msgget ID_kolejki_ratownik_przyjmuje");
    }

    // Utworzenie kolejki do wypuszczania klientów
    key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
    if(klucz_kolejki_ratownik_wypuszcza == -1)
    {
        handle_error("ftok klucz_kolejki_ratownik_wypuszcza");
    }
    ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_wypuszcza == -1)
    {
        handle_error("msgget ID_kolejki_ratownik_wypuszcza");
    }

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
  

    // Utworzenie wątków do przyjmowania i wypuszczania klientów
    if(pthread_create(&przyjmuje, NULL, przyjmowanie, NULL) != 0)
    {
        handle_error("pthread_create przyjmuje");
    }
    if(pthread_create(&wypuszcza, NULL, wypuszczanie, NULL) != 0)
    {
        handle_error("pthread_create wypuszcza");
    }

    pthread_join(przyjmuje, NULL);
    pthread_join(wypuszcza, NULL);

    return 0;
}

void* przyjmowanie()
{
    struct komunikat odebrany, wyslany;

    while(1)
    {
        print_klienci_w_basenie();

        if(msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_BRODZIK, 0) == -1)
        {
            handle_error("msgrcv ratownik brodzik przyjmuje");
        }

        if(odebrany.wiek <= 5)
        {
            pthread_mutex_lock(&klient_mutex);
            klienci_w_basenie[licznik_klientow++] = odebrany.PID;
            pthread_mutex_unlock(&klient_mutex);

            wyslany.mtype = odebrany.PID;
            wyslany.PID = odebrany.PID;
            wyslany.pozwolenie = true;
            wyslany.id_semafora = ID_semafora_brodzik;

            if(msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
            {
                handle_error("msgsnd brodzik ratownik");
            }

            printf("%s[%s] Ratownik brodzika przyjął klienta %d z opiekunem%s\n", COLOR4, timestamp(), odebrany.PID, RESET);
        }
        else
        {
            wyslany.mtype = odebrany.PID;
            wyslany.PID = odebrany.PID;
            wyslany.pozwolenie = false;

            if(msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
            {
                handle_error("msgsnd brodzik ratownik przyjmuje");
            }

            printf("%s[%s] Ratownik brodzika nie przyjął klienta %d%s\n", COLOR4, timestamp(), odebrany.PID, RESET);
        }
    }
}

void* wypuszczanie()
{
    struct komunikat odebrany, wyslany;

    while(1)
    {
        print_klienci_w_basenie();

        if(msgrcv(ID_kolejki_ratownik_wypuszcza, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_BRODZIK, 0) == -1)
        {
            handle_error("msgrcv ratownik brodzik wypuszcza");
        }

        pthread_mutex_lock(&klient_mutex);

        int indeks_klienta_do_usuniecia;
        for (int i = 0; i < licznik_klientow; i++)
        {
            if (klienci_w_basenie[i] == odebrany.PID)
            {
                indeks_klienta_do_usuniecia = i;
                break;
            }
        }

        for (int i = indeks_klienta_do_usuniecia; i < licznik_klientow - 1; i++)
        {
            klienci_w_basenie[i] = klienci_w_basenie[i + 1];
        }
        licznik_klientow--;
        
        pthread_mutex_unlock(&klient_mutex);

        wyslany.mtype = odebrany.PID;
        wyslany.PID = odebrany.PID;
        wyslany.id_semafora = ID_semafora_brodzik;

        if(msgsnd(ID_kolejki_ratownik_wypuszcza, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
        {
            handle_error("msgsnd ID_kolejki_ratownik_wypuszcza");
        }

        printf("%s[%s] Ratownik brodzika wypuścił klienta %d z opiekunem%s\n", COLOR4, timestamp(), odebrany.PID, RESET);
    }
}