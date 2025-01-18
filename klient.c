#include "utils.c"

struct dane_klienta
{
    int PID;
    int wiek;
    int wiek_opiekuna;
    int VIP;
};

int main()
{
    // Tablica do timestampów
    char time_str[9];

    int ratownik_przyjmuje, ratownik_wypuszcza;

    srand(getpid());

    struct dane_klienta klient;
    klient.PID = getpid();
    klient.wiek = (rand() % 70) + 1;
    klient.VIP = (rand() % 2) + 1;
    if(klient.wiek<=10)
    {
        klient.wiek_opiekuna = (rand()%53) + 19;
        if(klient.wiek<=3)
        {
            printf("[%s] Klientowi %d opiekun założył pampersa\n", timestamp(), getpid());
        }
    }
    int wybor_basenu = rand()%3 + 1;
    
    if(wybor_basenu == 1)
    {
        ratownik_przyjmuje = RATOWNIK_OLIMPIJSKI_PRZYJMUJE;
        ratownik_wypuszcza = RATOWNIK_OLIMPIJSKI_WYPUSZCZA;
    }
    else if(wybor_basenu == 2)
    {
        ratownik_przyjmuje = RATOWNIK_REKREACYJNY_PRZYJMUJE;
        ratownik_wypuszcza = RATOWNIK_REKREACYJNY_WYPUSZCZA;
    }
    else
    {
        ratownik_przyjmuje = RATOWNIK_BRODZIK_PRZYJMUJE;
        ratownik_wypuszcza = RATOWNIK_BRODZIK_WYPUSZCZA;
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
    if(klient.VIP==1)
    {
        wyslany.mtype = 1;
    }
    if(klient.VIP==2)
    {
        wyslany.mtype = 2;
    }

    wyslany.ktype = getpid();
    wyslany.wiek = klient.wiek;
    wyslany.wiek_opiekuna = klient.wiek_opiekuna;
    printf("%d wysyłam zapytanie do kasjera. Wiek: %d. Wiek opiekuna: %d\n", getpid(), klient.wiek, klient.wiek_opiekuna);

	// Klient wysyła wiadomość do kasjera
	msgsnd(ID_kolejki_kasjer, &wyslany, sizeof(wyslany) - sizeof(long), 0);

    // Kasjer odsyła wiadomość na PID klienta, dlatego aby odebrać wiadomość klient ustawia ktype na swoje PID
    odebrany.ktype = getpid();

    // Klient odbiera wiadomość zwrotną od kasjera
	msgrcv(ID_kolejki_kasjer, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.ktype, 0);
    if (odebrany.moze_wejsc = true)
    {
        printf("%d zostałem przyjęty\n");
    }

    // Komunikacja z ratownikiem (klient chce wejść na basen)

    // Utworzenie kolejki dla ratownika
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    int ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);

    // Klient ustawia takie wartości, aby wiadomość dotarła do ratownika
    wyslany.mtype = ratownik_przyjmuje;
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
        time_t czas_wyjscia = time(NULL) + BILET;

        while (time(NULL) < czas_wyjscia)
        {
            sleep(1);
        }

        // Komunikacja z ratownikiem (klient chce wyjść z basenu)

        // Utworzenie kolejki dla ratownika
        key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
        int ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600);

        // Klient ustawia takie wartości, aby wiadomość dotarła do ratownika
        wyslany.mtype = ratownik_wypuszcza;
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