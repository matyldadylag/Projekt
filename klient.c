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
#include <time.h>
#include <sys/shm.h>

// TODO Okreslic wartosci dla kolejki komunikatow
#define MAKS_DLUGOSC_KOMUNIKATU 255
#define KASJER 1
#define RATOWNIK1 2
#define RATOWNIK2 3
#define CZAS_BILETU 15

// Struktura komunikatu
struct komunikat
{
    long mtype;
    pid_t ktype;
    char mtext[MAKS_DLUGOSC_KOMUNIKATU];
};

int main()
{
    // Uzyskanie dostępu do pamięci dzielonej do przechowywania zmiennej czas_otwarcia
    key_t klucz_pamieci = ftok(".", 3213);
    int ID_pamieci = shmget(klucz_pamieci, sizeof(time_t), 0600 | IPC_CREAT);
    time_t* czas_otwarcia = (time_t*)shmat(ID_pamieci, NULL, 0);

    // Deklaracja struktur do wysyłania i odbierania wiadomości od kasjera i ratownika
    struct komunikat wyslany, odebrany;

    // Komunikacja z kasjerem

    // Uzyskanie dostępu do kolejki dla kasjera
    key_t klucz_kolejki_kasjer = ftok(".", 6377);
    int ID_kolejki_kasjer = msgget(klucz_kolejki_kasjer, IPC_CREAT | 0600);

    // Klient ustawia takie wartości, aby wiadomość dotarła do kasjera
    wyslany.mtype = KASJER;
    wyslany.ktype = getpid();
    sprintf(wyslany.mtext, "[%.0f] Klient->Kasjer: jestem %d i chcę zapłacić\n", difftime(time(NULL), *czas_otwarcia), wyslany.ktype);

	// Klient wysyła wiadomość do kasjera
	msgsnd(ID_kolejki_kasjer, &wyslany, sizeof(wyslany) - sizeof(long), 0);

    // Kasjer odsyła wiadomość na PID klienta, dlatego aby odebrać wiadomość klient ustawia ktype na swoje PID
    odebrany.ktype = getpid();

    // Klient odbiera wiadomość zwrotną od kasjera
	msgrcv(ID_kolejki_kasjer, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.ktype, 0);
	printf("%s", odebrany.mtext);

    // Komunikacja z ratownikiem (klient chce wejść na basen)

    // Utworzenie kolejki dla ratownika
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    int ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);

    // Klient ustawia takie wartości, aby wiadomość dotarła do ratownika
    wyslany.mtype = RATOWNIK1;
    wyslany.ktype = getpid();
    sprintf(wyslany.mtext, "[%.0f] Klient->Ratownik: jestem %d i chcę wejść na basen\n", difftime(time(NULL), *czas_otwarcia), wyslany.ktype);

	// Klient wysyła wiadomość do ratownika
	msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(wyslany) - sizeof(long), 0);

    // Ratownik odsyła wiadomość na PID klienta, dlatego aby odebrać wiadomość klient ustawia ktype na swoje PID
    odebrany.ktype = getpid();

    // Klient odbiera wiadomość zwrotną od ratownika
	msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.ktype, 0);
	printf("%s", odebrany.mtext);

    // TODO Klient pilnuje ile czasu jest na basenie i kiedy musi wyjść
    time_t czas_wyjscia = time(NULL) + CZAS_BILETU;

    while (time(NULL) < czas_wyjscia)
    {
        sleep(1);
    }

    // Komunikacja z ratownikiem (klient chce wyjść z basenu)

    // Utworzenie kolejki dla ratownika
    key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
    int ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600);

    // Klient ustawia takie wartości, aby wiadomość dotarła do ratownika
    wyslany.mtype = RATOWNIK2;
    wyslany.ktype = getpid();
    sprintf(wyslany.mtext, "[%.0f] Klient->Ratownik: jestem %d i chcę wyjść z basenu\n", difftime(time(NULL), *czas_otwarcia), wyslany.ktype);

	// Klient wysyła wiadomość do ratownika
	msgsnd(ID_kolejki_ratownik_wypuszcza, &wyslany, sizeof(wyslany) - sizeof(long), 0);

    // Ratownik odsyła wiadomość na PID klienta, dlatego aby odebrać wiadomość klient ustawia ktype na swoje PID
    odebrany.ktype = getpid();

    // Klient odbiera wiadomość zwrotną od ratownika
	msgrcv(ID_kolejki_ratownik_wypuszcza, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.ktype, 0);
	printf("%s", odebrany.mtext);

	return 0;
}