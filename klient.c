#include "utils.c"

void SIGINT_handler(int sig)
{
    // Komunikat o zakończeniu działania klienta
    printf("%s[%s] Klient %d kończy działanie%s\n", GREEN, timestamp(), getpid(), RESET);

    exit(0);
}

int main()
{
    // Obsługa SIGINT
    signal(SIGINT, SIGINT_handler);

    // Punkt początkowy do losowania na podstawie PID procesu
    srand(getpid());

    // Zainicjowanie danych klienta
    struct dane_klienta klient;
    klient.PID = getpid();
    klient.wiek = (rand() % 70) + 1; // Losowo generuje wiek od 1 do 70 lat
    klient.wiek_opiekuna = (klient.wiek <= 10) ? ((rand() % 53) + 19) : 0; // W wypadku, gdy klient.wiek <= 10 generuje się wiek opiekuna (między 18 a 70 lat)
    klient.VIP = (rand() % 5 == 0); // Klient ma szansę 1:5 na bycie VIP
    klient.czepek = (rand() % 5 == 0); // Klient ma szansę 1:5 na założenie czepka
    // Wybór basenu (1 - brodzik, 2 - rekreacyjny, 3 - olimpijski)
    if (klient.wiek <= 5)
    {
        klient.wybor_basenu = (rand() % 2) + 1; // Jeśli klient ma <=5 lat może wybrać między basenem 1 i 2
    }
    else if (klient.wiek <= 18)
    {
        klient.wybor_basenu = 2; // Jeśli klient ma pomiędzy 5 a 18 lat może wybrać tylko basen 2
    }
    else
    {
        klient.wybor_basenu = (rand() % 2) + 2; // Jeśli klient ma 18+ lat może wybrać między basen 2 i 3
    }

    // Komunikat o uruchomieniu klienta
    if(klient.wiek_opiekuna == 0)
    {
        printf("%s[%s] Klient %d uruchomiony. Wiek: %d. VIP: %d. Czepek: %d\n%s", GREEN, timestamp(), klient.PID, klient.wiek, klient.VIP, klient.czepek, RESET);
    }
    else
    {
        printf("%s[%s] Klient %d uruchomiony. Wiek: %d. Wiek opiekuna: %d. VIP: %d. Czepek: %d\n%s", GREEN, timestamp(), klient.PID, klient.wiek, klient.wiek_opiekuna, klient.VIP, klient.czepek, RESET);
    }
    
    // Deklaracja struktur do wysyłania i odbierania wiadomości od kasjera i ratownika
    struct komunikat wyslany, odebrany;

    // Komunikacja z kasjerem
    key_t klucz_kolejki_kasjer = ftok(".", 6377);
    int ID_kolejki_kasjer = msgget(klucz_kolejki_kasjer, IPC_CREAT | 0600); // Uzyskanie dostępu do kolejki kasjera

    // Ustawienie wartości struktury wysłanej przez komunikat
    if(klient.VIP==1) // W zależności od tego, czy klient jest VIP czy nie, ustawia inny mtype
    {
        wyslany.mtype = KASJER_VIP;
    }
    else
    {
        wyslany.mtype = KASJER;
    }
    wyslany.PID = klient.PID;
    wyslany.wiek = klient.wiek;
    wyslany.wiek_opiekuna = klient.wiek_opiekuna;
    // Wysyłanie komunikatu do kasjera
	msgsnd(ID_kolejki_kasjer, &wyslany, sizeof(wyslany) - sizeof(long), 0);

    // Ustawienie wartości do odebrania komunikatu od kasjera
    odebrany.PID = wyslany.PID;
    // Odbieranie komunikatu od kasjera
	msgrcv(ID_kolejki_kasjer, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.PID, 0);
    if (odebrany.pozwolenie == false) // Jeśli klient nie dostał pozwolenia na wejście, kończy swoje działanie
    {
        raise(SIGINT);
    }

    // Komunikacja z ratownikiem    
    // W zależności od wyboru basenu, klient wybiera z którym ratownik będzie się komunikował
    int ratownik;
    if(klient.wybor_basenu == 1)
    {
        ratownik = RATOWNIK_BRODZIK;
    }
    else if(klient.wybor_basenu == 2)
    {
        ratownik = RATOWNIK_REKREACYJNY;
    }
    else
    {
        ratownik = RATOWNIK_OLIMPIJSKI;
    }


    // Utworzenie kolejki dla ratownika
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    int ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);

    // Klient ustawia takie wartości, aby wiadomość dotarła do ratownika
    wyslany.mtype = ratownik;
    wyslany.PID = klient.PID;
    wyslany.wiek = klient.wiek;
    wyslany.wiek_opiekuna = klient.wiek_opiekuna;

	// Klient wysyła wiadomość do ratownika
    printf("[%s] Klient: jestem %d. Wiek: %d. Chcę wejść na basen\n", timestamp(), getpid(), klient.wiek, wyslany.PID);
	msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(wyslany) - sizeof(long), 0);

    // Ratownik odsyła wiadomość na PID klienta, dlatego aby odebrać wiadomość klient ustawia ktype na swoje PID
    odebrany.PID = getpid();

    // Klient odbiera wiadomość zwrotną od ratownika
	msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.PID, 0);

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
        wyslany.mtype = ratownik;
        wyslany.PID = getpid();
        sprintf(wyslany.mtext, "[%s] Klient->Ratownik: jestem %d i chcę wyjść z basenu\n", timestamp(), wyslany.PID);

        // Klient wysyła wiadomość do ratownika
        msgsnd(ID_kolejki_ratownik_wypuszcza, &wyslany, sizeof(wyslany) - sizeof(long), 0);

        // Ratownik odsyła wiadomość na PID klienta, dlatego aby odebrać wiadomość klient ustawia ktype na swoje PID
        odebrany.PID = getpid();

        // Klient odbiera wiadomość zwrotną od ratownika
        msgrcv(ID_kolejki_ratownik_wypuszcza, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.PID, 0);
        printf("%s", odebrany.mtext);
    }
    else
    {
        printf("[%s] KLIENT NIEPRZYJĘTY %d\n%s", timestamp(), getpid(), odebrany.mtext); 
    }

    // Gdy klient zakończy działanie wywołuje SIGINT
    raise(SIGINT);

	return 0;
}