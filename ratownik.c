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

// TODO Okreslic wartosci dla kolejki komunikatow
#define MAKS_DLUGOSC_KOMUNIKATU 255
#define KASJER 1
#define RATOWNIK 2

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

// Zdefiniowane wyżej, aby być w stanie usunąć struktury asynchronicznie w przypadku SIGINT
int ID_kolejki;
int ID_semafora;
pthread_t przyjmuje, wypuszcza;

// Obsługa SIGINT
void SIGINT_handler(int sig)
{
    // Anulowanie wątków przyjmowania i wypuszczania klientów
    pthread_kill(przyjmuje, SIGINT);
    // TODO pthread_kill(wypuszcza, SIGINT);

    // Usunięcie struktur
    msgctl(ID_kolejki, IPC_RMID, 0);
    semctl(ID_semafora, 0, IPC_RMID);

    exit(0);
}

int main()
{
    // Obsługa SIGINT
    signal(SIGINT, SIGINT_handler);
    
    // Utworzenie kolejki
    key_t klucz_kolejki = ftok(".", 7942);
    ID_kolejki = msgget(klucz_kolejki, IPC_CREAT | 0600);

    // Utworzenie semafora
    key_t klucz_semafora = ftok(".", 9447);
    ID_semafora = semget(klucz_semafora, 1, 0600|IPC_CREAT);
    semctl(ID_semafora, 0, SETVAL, 2);

    // Utworzenie wątków do przyjmowania i wypuszczania klientów
    pthread_create(&przyjmuje, NULL, przyjmowanie, NULL);
    // TODO pthread_create(&wypuszcza, NULL, wypuszczanie, NULL);

    pthread_join(przyjmuje, NULL);

    return 0;
}

void* przyjmowanie()
{
    // Deklaracja struktur do odbierania i wysyłania wiadomości od klientów
    struct komunikat odebrany, wyslany;

    while(1)
    {
        msgrcv(ID_kolejki, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK, 0);
        printf("%s", odebrany.mtext);

        // Przyjęcie klienta na basen
        semafor_p(ID_semafora, 0);

        // Ratownik zmienia wartości, aby wiadomość dotarła na PID klienta
        wyslany.mtype = odebrany.ktype;
        wyslany.ktype = odebrany.ktype;

        // Utworzenie wiadomości
        sprintf(wyslany.mtext, "Ratownik->Klient: przyjmuję %d na basen\n", odebrany.ktype);

        // Wysłanie wiadomości
        msgsnd(ID_kolejki, &wyslany, sizeof(struct komunikat) - sizeof(long), 0);

        // TODO na potrzeby testu zwalniam semafor zaraz po
        sleep(3);
        semafor_v(ID_semafora, 0);
    }
}

/*TODO void* wypuszczanie()
{
}*/