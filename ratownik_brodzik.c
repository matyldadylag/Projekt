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

    // Usunięcie struktur
    msgctl(ID_kolejki_ratownik_przyjmuje, IPC_RMID, 0);
    msgctl(ID_kolejki_ratownik_wypuszcza, IPC_RMID, 0);
    semctl(ID_semafora, 0, IPC_RMID);

    // Komunikat o zakończeniu działania ratownika brodzika
    //printf("%s[%s] Ratownik brodzika kończy działanie%s\n", COLOR4, timestamp(), RESET);

    exit(0);
}

int main()
{
    // Obsługa SIGINT
    signal(SIGINT, SIGINT_handler);
  
    // Komunikat o uruchomieniu ratownika brodzika
    //printf("%s[%s] Ratownik brodzika uruchomiony%s\n", COLOR4, timestamp(), RESET);

    // Utworzenie kolejki do przyjmowania klientów
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);

    // Utworzenie kolejki do wypuszczania klientów
    key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
    ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600);

    // Utworzenie semafora
    key_t klucz_semafora = ftok(".", 3293);
    ID_semafora = semget(klucz_semafora, 1, 0600|IPC_CREAT);
    semctl(ID_semafora, 0, SETVAL, MAKS_BRODZIK);

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
        msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_BRODZIK, 0);
        
        int wiek = odebrany.wiek;

        if(wiek<=5)
        {
            //printf("%s[%s] Ratownik brodzika przyjął klienta %d z opiekunem%s\n", COLOR4, timestamp(), odebrany.PID, RESET);

            // Przyjęcie klienta na basen
            semafor_p_2(ID_semafora, 0);

            // Dodanie PID klienta do tablicy
            // Blokada przez muteks
            pthread_mutex_lock(&klient_mutex);
            klienci_w_basenie[licznik_klientow++] = odebrany.PID;
            pthread_mutex_unlock(&klient_mutex);

            // Ratownik zmienia wartości, aby wiadomość dotarła na PID klienta
            wyslany.mtype = odebrany.PID;
            wyslany.PID = odebrany.PID;
            wyslany.pozwolenie_ratownik = true;

            // Wysłanie wiadomości
            msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0);
        }
        else
        {
            //printf("%s[%s] Ratownik brodzika nie przyjął klienta %d%s\n", COLOR4, timestamp(), odebrany.PID, RESET);

            // Ratownik zmienia wartości, aby wiadomość dotarła na PID klienta
            wyslany.mtype = odebrany.PID;
            wyslany.PID = odebrany.PID;
            wyslany.pozwolenie_ratownik = false;

            // Wysłanie wiadomości
            msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0);
        }
    }
}

void* wypuszczanie()
{
    struct komunikat odebrany, wyslany;

    while(1)
    {
        msgrcv(ID_kolejki_ratownik_wypuszcza, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_BRODZIK, 0);

        //printf("%s[%s] Ratownik brodzika wypuścił klienta %d z opiekunem%s\n", COLOR4, timestamp(), odebrany.PID, RESET);

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
        semafor_v_2(ID_semafora, 0);

        // Ratownik zmienia wartości, aby wiadomość dotarła na PID klienta
        wyslany.mtype = odebrany.PID;
        wyslany.PID = odebrany.PID;

        // Wysłanie wiadomości
        msgsnd(ID_kolejki_ratownik_wypuszcza, &wyslany, sizeof(struct komunikat) - sizeof(long), 0);
    }
}