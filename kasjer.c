// TODO Sprawdzic, ktore pliki naglowkowe sa potrzebne
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>

// TODO Okreslic wartosci dla kolejki komunikatow
#define MAX 255
#define KASJER 1
#define RATOWNIK 2

// Zdefiniowane wyżej, aby być w stanie usunąć struktury asynchronicznie w przypadku SIGINT
pid_t ID_kolejki_kasjer;

// Obsługa SIGINT
void SIGINT_handler(int sig)
{
    // Usunięcie kolejki komunikatów
    msgctl(ID_kolejki_kasjer, IPC_RMID, 0);

    exit(0);
}

// Struktura komunikatu
struct komunikat
{
    long mtype;
    pid_t ktype;
    char mtext[MAX];
};

int main()
{
    signal(SIGINT, SIGINT_handler);
    
    // Utworzenie kolejki dla kasjera
    key_t klucz_kolejki_kasjer = ftok(".", 6377);
    ID_kolejki_kasjer = msgget(klucz_kolejki_kasjer, IPC_CREAT | 0600);

    struct komunikat odebrany, wyslany;

    while(1)
    {
        // Odebranie wiadomości od klienta
        msgrcv(ID_kolejki_kasjer, &odebrany, sizeof(odebrany) - sizeof(long), KASJER, 0);
        printf("%s", odebrany.mtext);

        // Kasjer zmienia wartości, aby wiadomość dotarła na PID klienta
        wyslany.mtype = odebrany.ktype;
        wyslany.ktype = odebrany.ktype;

        // Utworzenie wiadomości
        sprintf(wyslany.mtext, "Kasjer->Klient: przyjmuję płatność %d\n", odebrany.ktype);

        // Wysłanie wiadomości
        msgsnd(ID_kolejki_kasjer, &wyslany, sizeof(struct komunikat) - sizeof(long), 0);
    }

    return 0;
}