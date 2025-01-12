// TODO Sprawdzic, ktore pliki naglowkowe sa potrzebne
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/shm.h>

// TODO Okreslic wartosci dla kolejki komunikatow
#define MAKS_LICZBA_KLIENTOW 2
#define MAKS_DLUGOSC_KOMUNIKATU 255
#define KASJER 1
#define RATOWNIK1 2
#define RATOWNIK2 3

// Struktura komunikatu
struct komunikat
{
    long mtype;
    pid_t ktype;
    char mtext[MAKS_DLUGOSC_KOMUNIKATU];
};

// Funkcje semafora
static void semafor_v(int semafor_id, int numer_semafora)
{
    struct sembuf bufor_sem;
    bufor_sem.sem_num = numer_semafora;
    bufor_sem.sem_op = 1;
    bufor_sem.sem_flg = SEM_UNDO;

    semop(semafor_id, &bufor_sem, 1);
}

static void semafor_p(int semafor_id, int numer_semafora)
{
    struct sembuf bufor_sem;
    bufor_sem.sem_num = numer_semafora;
    bufor_sem.sem_op = -1;
    bufor_sem.sem_flg = 0;
    
    semop(semafor_id, &bufor_sem, 1);
}

// Funkcje dla wątków
void* przyjmowanie();
void* wypuszczanie();

// Zdefiniowane wyżej, aby być w stanie usunąć struktury asynchronicznie w przypadku SIGINT
int ID_kolejki_ratownik_przyjmuje;
int ID_kolejki_ratownik_wypuszcza;
int ID_semafora;
pthread_t przyjmuje, wypuszcza;
time_t* czas_otwarcia;

// Tablica przechowująca PID klientów aktualnie na basenie
pid_t klienci_w_basenie[MAKS_LICZBA_KLIENTOW];
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

    exit(0);
}

int main()
{
    // Obsługa SIGINT
    signal(SIGINT, SIGINT_handler);

    // Uzyskanie dostępu do pamięci dzielonej do przechowywania zmiennej czas_otwarcia
    key_t klucz_pamieci = ftok(".", 3213);
    int ID_pamieci = shmget(klucz_pamieci, sizeof(time_t), 0600 | IPC_CREAT);
    czas_otwarcia = (time_t*)shmat(ID_pamieci, NULL, 0);
    
    // Utworzenie kolejki do przyjmowania klientów
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);

    // Utworzenie kolejki do wypuszczania klientów
    key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
    ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600);

    // Utworzenie semafora
    key_t klucz_semafora = ftok(".", 9447);
    ID_semafora = semget(klucz_semafora, 1, 0600|IPC_CREAT);
    semctl(ID_semafora, 0, SETVAL, MAKS_LICZBA_KLIENTOW);

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
        msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK1, 0);
        printf("%s", odebrany.mtext);

        // Przyjęcie klienta na basen
        semafor_p(ID_semafora, 0);

        // Dodanie PID klienta do tablicy
        // Blokada przez muteks
        pthread_mutex_lock(&klient_mutex);
        klienci_w_basenie[licznik_klientow++] = odebrany.ktype;
        pthread_mutex_unlock(&klient_mutex);

        // Ratownik zmienia wartości, aby wiadomość dotarła na PID klienta
        wyslany.mtype = odebrany.ktype;
        wyslany.ktype = odebrany.ktype;

        // Utworzenie wiadomości
        sprintf(wyslany.mtext, "[%.0f] Ratownik->Klient: przyjmuję %d na basen\n", difftime(time(NULL), *czas_otwarcia), odebrany.ktype);

        // Wysłanie wiadomości
        msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0);
    }
}

void* wypuszczanie()
{
    struct komunikat odebrany, wyslany;

    while(1)
    {
        msgrcv(ID_kolejki_ratownik_wypuszcza, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK2, 0);
        printf("%s", odebrany.mtext);

        // Usuwanie PID klienta z tablicy
        // Blokada przez muteks
        pthread_mutex_lock(&klient_mutex);

        // Znalezienie indeksu przy którym znajduje się PID klienta, który wychodzi
        int indeks_klienta_do_usuniecia;
        for (int i = 0; i < licznik_klientow; i++)
        {
            if (klienci_w_basenie[i] == odebrany.ktype)
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
        wyslany.mtype = odebrany.ktype;
        wyslany.ktype = odebrany.ktype;

        // Utworzenie wiadomości
        sprintf(wyslany.mtext, "[%.0f] Ratownik->Klient: wypuszczam %d z basenu\n", difftime(time(NULL), *czas_otwarcia), odebrany.ktype);

        // Wysłanie wiadomości
        msgsnd(ID_kolejki_ratownik_wypuszcza, &wyslany, sizeof(struct komunikat) - sizeof(long), 0);
    }
}