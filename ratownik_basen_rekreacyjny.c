#include "utils.c"

// Funkcje dla wątków
void* przyjmowanie();
void* wypuszczanie();

// Zdefiniowane wyżej, aby być w stanie usunąć struktury asynchronicznie w przypadku SIGINT
int ID_kolejki_ratownik_przyjmuje;
int ID_kolejki_ratownik_wypuszcza;
int ID_semafora;
pthread_t przyjmuje, wypuszcza;
double suma_wieku;

// Tablica przechowująca PID klientów aktualnie na basenie
pid_t klienci_w_basenie[MAKS_REKREACYJNY];
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

    // Usunięcie struktury
    if(semctl(ID_semafora, 0, IPC_RMID)==-1)
    {
        handle_error("semctl ID_semafora");
    }

    // Komunikat o zakończeniu działania ratownika basenu rekreacyjnego
    printf("%s[%s] Ratownik basenu rekreacyjnego kończy działanie%s\n", COLOR5, timestamp(), RESET);

    exit(0);
}

void print_klienci_w_basenie()
{
    pthread_mutex_lock(&klient_mutex);
    printf("%s[%s] Basen rekreacyjny: [", COLOR5, timestamp());
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

    // Komunikat o uruchomieniu ratownika basenu rekreacyjnego
    printf("%s[%s] Ratownik basenu rekreacyjnego uruchomiony%s\n", COLOR5, timestamp(), RESET);
    
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

    // Utworzenie semafora
    key_t klucz_semafora = ftok(".", 2003);
    if(klucz_semafora == -1)
    {
        handle_error("ftok klucz_semafora");
    }
    ID_semafora = semget(klucz_semafora, 1, 0600 | IPC_CREAT);
    if(ID_semafora == -1)
    {
        handle_error("semget ID_semafora");
    }
    if(semctl(ID_semafora, 0, SETVAL, MAKS_REKREACYJNY) == -1)
    {
        handle_error("semctl SETVAL");
    }

    // Utworzenie wątków do przyjmowania i wypuszczania klientów
    if(pthread_create(&przyjmuje, NULL, przyjmowanie, NULL) != 0)
    {
        handle_error("pthread_create przyjmowanie");
    }
    if(pthread_create(&wypuszcza, NULL, wypuszczanie, NULL) != 0)
    {
        handle_error("pthread_create wypuszczanie");
    }

    if(pthread_join(przyjmuje, NULL) != 0)
    {
        handle_error("pthread_join przyjmowanie");
    }
    if(pthread_join(wypuszcza, NULL) != 0)
    {
        handle_error("pthread_join wypuszczanie");
    }

    return 0;
}

void* przyjmowanie()
{
    // Deklaracja struktur do odbierania i wysyłania wiadomości od klientów
    struct komunikat odebrany, wyslany;
    double temp;

    while(1)
    {
        if(msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_REKREACYJNY, 0) == -1)
        {
            handle_error("msgrcv ratownik rekreacyjny przyjmuje");
        }
        
        // Wykorzystuję zmienną temp, bo jeszcze nie wiem czy klienta przyjmę - sprawdzam czy średnia będzie za wysoka
        temp = (suma_wieku + odebrany.wiek)/(licznik_klientow+1);
        
        if(temp <= 40)
        {
            printf("%s[%s] Ratownik basenu rekreacyjnego przyjął klienta %d%s\n", COLOR5, timestamp(), odebrany.PID, RESET);

            // Przyjęcie klienta na basen
            suma_wieku += odebrany.wiek;
            semafor_p(ID_semafora, 0);
           
            // Dodanie PID klienta do tablicy
            pthread_mutex_lock(&klient_mutex);
            klienci_w_basenie[licznik_klientow++] = odebrany.PID;
            pthread_mutex_unlock(&klient_mutex);

            wyslany.mtype = odebrany.PID;
            wyslany.PID = odebrany.PID;
            wyslany.pozwolenie = true;

            if(msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
            {
                handle_error("msgsnd ID_kolejki_ratownik_przyjmuje");
            }
        }
        else
        {
            printf("%s[%s] Ratownik basenu rekreacyjnego nie przyjął klienta %d%s\n", COLOR5, timestamp(), odebrany.PID, RESET);

            wyslany.mtype = odebrany.PID;
            wyslany.PID = odebrany.PID;
            wyslany.pozwolenie = false;

            if(msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
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

        if(msgrcv(ID_kolejki_ratownik_wypuszcza, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_REKREACYJNY, 0) == -1)
        {
            handle_error("msgrcv ratownik rekreacyjny wpuszcza");
        }

        printf("%s[%s] Ratownik basenu rekreacyjnego wypuścił klienta %d%s\n", COLOR5, timestamp(), odebrany.PID, RESET);
        
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

        semafor_v(ID_semafora, 0);

        suma_wieku -= odebrany.wiek;

        wyslany.mtype = odebrany.PID;
        wyslany.PID = odebrany.PID;

        if(msgsnd(ID_kolejki_ratownik_wypuszcza, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
        {
            handle_error("msgsnd ID_kolejki_ratownik_wypuszcza");
        }
    }
}
