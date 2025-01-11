// TODO Sprawdzic, ktore pliki naglowkowe sa potrzebne
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>

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

int main()
{
    // Deklaracja struktur do wysyłania i odbierania wiadomości od kasjera i ratownika
    struct komunikat wyslany, odebrany;

    // Komunikacja z kasjerem

    // Uzyskanie dostępu do kolejki dla kasjera
    key_t klucz_kolejki_kasjer = ftok(".", 6377);
    int ID_kolejki_kasjer = msgget(klucz_kolejki_kasjer, IPC_CREAT | 0600);
    
    // Klient ustawia takie wartości, aby wiadomość dotarła do kasjera
    wyslany.mtype = KASJER;
    wyslany.ktype = getpid();
    sprintf(wyslany.mtext, "Klient->Kasjer: jestem %d i chcę zapłacić\n", wyslany.ktype);

	// Klient wysyła wiadomość do kasjera
	msgsnd(ID_kolejki_kasjer, &wyslany, sizeof(wyslany) - sizeof(long), 0);

    // Kasjer odsyła wiadomość na PID klienta, dlatego aby odebrać wiadomość klient ustawia ktype na swoje PID
    odebrany.ktype = getpid();

    // Klient odbiera wiadomość zwrotną od kasjera
	msgrcv(ID_kolejki_kasjer, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.ktype, 0);
	printf("%s", odebrany.mtext);

    // Komunikacja z ratownikiem

    // Utworzenie kolejki dla ratownika
    key_t klucz_kolejki_ratownik = ftok(".", 7942);
    int ID_kolejki_ratownik = msgget(klucz_kolejki_ratownik, IPC_CREAT | 0600);

    // Klient ustawia takie wartości, aby wiadomość dotarła do ratownika
    wyslany.mtype = RATOWNIK;
    wyslany.ktype = getpid();
    sprintf(wyslany.mtext, "Klient->Ratownik: jestem %d i chcę wejść na basen\n", wyslany.ktype);

	// Klient wysyła wiadomość do ratownika
	msgsnd(ID_kolejki_ratownik, &wyslany, sizeof(wyslany) - sizeof(long), 0);

    // Ratownik odsyła wiadomość na PID klienta, dlatego aby odebrać wiadomość klient ustawia ktype na swoje PID
    odebrany.ktype = getpid();

    // Klient odbiera wiadomość zwrotną od ratownika
	msgrcv(ID_kolejki_ratownik, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.ktype, 0);
	printf("%s", odebrany.mtext);

	return 0;
}