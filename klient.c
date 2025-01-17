#include "header.h"

struct dane_klienta
{
    int PID;
    int wiek;
    int wiek_opiekuna;
};

int main()
{
    // Tablica do timestampów
    char time_str[9];

    srand(getpid());

    struct dane_klienta klient;
    klient.PID = getpid();
    klient.wiek = (rand() % 70) + 1;
    if(klient.wiek<=10)
    {
        klient.wiek_opiekuna = (rand()%53) + 19;
        if(klient.wiek<=3)
        {
            printf("[%s] Klientowi %d opiekun założył pampersa\n", timestamp(), getpid());
        }
    }

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
    sprintf(wyslany.mtext, "[%s] Klient->Kasjer: jestem %d i chcę zapłacić\n", timestamp(), wyslany.ktype);

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
    wyslany.mtype = RATOWNIK_OLIMPIJSKI_PRZYJMUJE;
    wyslany.ktype = getpid();
    sprintf(wyslany.mtext, "%d", klient.wiek);

	// Klient wysyła wiadomość do ratownika
    printf("[%s] Klient: jestem %d. Wiek: %d. Chcę wejść na basen\n", timestamp(), getpid(), klient.wiek, wyslany.ktype);
	msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(wyslany) - sizeof(long), 0);

    // Ratownik odsyła wiadomość na PID klienta, dlatego aby odebrać wiadomość klient ustawia ktype na swoje PID
    odebrany.ktype = getpid();

    // Klient odbiera wiadomość zwrotną od ratownika
	msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.ktype, 0);

    if(strcmp(odebrany.mtext, "OK") == 0)
    {
        printf("[%s] Klient: jestem %d i wchodzę na basen\n", timestamp(), getpid());

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
        wyslany.mtype = RATOWNIK_OLIMPIJSKI_WYPUSZCZA;
        wyslany.ktype = getpid();
        sprintf(wyslany.mtext, "[%s] Klient->Ratownik: jestem %d i chcę wyjść z basenu\n", timestamp(), wyslany.ktype);

        // Klient wysyła wiadomość do ratownika
        msgsnd(ID_kolejki_ratownik_wypuszcza, &wyslany, sizeof(wyslany) - sizeof(long), 0);

        // Ratownik odsyła wiadomość na PID klienta, dlatego aby odebrać wiadomość klient ustawia ktype na swoje PID
        odebrany.ktype = getpid();

        // Klient odbiera wiadomość zwrotną od ratownika
        msgrcv(ID_kolejki_ratownik_wypuszcza, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.ktype, 0);
        printf("%s", odebrany.mtext);
    }
    else
    {
        printf("[%s] KLIENT NIEPRZYJĘTY %d\n%s", timestamp(), getpid(), odebrany.mtext); 
    }

	return 0;
}