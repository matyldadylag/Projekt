#include "utils.c"

time_t czas_wyjscia;
int ratownik;

void SIGINT_handler(int sig)
{
    // Komunikat o zakończeniu działania klienta
    printf("%s[%s] Klient %d kończy działanie%s\n", GREEN, timestamp(), getpid(), RESET);

    exit(0);
}

// Wątek mierzący czas - gdy zostanie osiągnięty czas wyjścia klienta z basenu, wysyła komunikat do ratownika o chęci wyjścia
void* czasomierz()
{
    // Klient pilnuje ile czasu jest na basenie i kiedy musi wyjść
    while (time(NULL) < czas_wyjscia)
    {
        sleep(1);
    }    

    return 0;
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
    // Wybór basenu (11 - brodzik, 12 - rekreacyjny, 13 - olimpijski)
    if (klient.wiek <= 5)
    {
        klient.wybor_basenu = (rand() % 2) + 11; // Jeśli klient ma <=5 lat może wybrać między basenem 11 i 12
    }
    else if (klient.wiek <= 18)
    {
        klient.wybor_basenu = 12; // Jeśli klient ma pomiędzy 5 a 18 lat może wybrać tylko basen 12
    }
    else
    {
        klient.wybor_basenu = (rand() % 2) + 12; // Jeśli klient ma 18+ lat może wybrać między basen 12 i 13
    }

    // Komunikat o uruchomieniu klienta
    if(klient.wiek_opiekuna == 0)
    {
        printf("%s[%s] Klient %d uruchomiony. Wiek: %d. VIP: %d. Czepek: %d. Preferowany basen: %d\n%s", GREEN, timestamp(), klient.PID, klient.wiek, klient.VIP, klient.czepek, klient.wybor_basenu, RESET);
    }
    else
    {
        printf("%s[%s] Klient %d uruchomiony. Wiek: %d. Wiek opiekuna: %d. VIP: %d. Czepek: %d. Preferowany basen: %d\n%s", GREEN, timestamp(), klient.PID, klient.wiek, klient.wiek_opiekuna, klient.VIP, klient.czepek, klient.wybor_basenu, RESET);
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

    // Odbieranie komunikatu od kasjera
    odebrany.PID = wyslany.PID; // Ustawienie wartości do odebrania komunikatu od kasjera
	msgrcv(ID_kolejki_kasjer, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.PID, 0);
    // Jeśli klient nie został przyjęty przez kasjera, kończy swoje działanie
    if(odebrany.pozwolenie == false)
    {
        raise(SIGINT);
    }

    // Klient wchodzi na basen, czas z biletu zaczyna upływać
    czas_wyjscia = time(NULL) + BILET;
    // Tworzy wątek pilnujący, kiedy kończy się bilet
    pthread_t czas;
    pthread_create(&czas, NULL, czasomierz, NULL);

    // Komunikat o założeniu pampersa
    if(klient.wiek<=3)
    {
        printf("%s[%s] Klientowi %d opiekun założył pampersa\n%s", GREEN, timestamp(), klient.PID, RESET);
    }

    // Komunikat o założeniu czepka
    if(klient.czepek == true)
    {
        printf("%s[%s] Klient %d założył czepek\n%s", GREEN, timestamp(), klient.PID, RESET);
    }

    // Komunikacja z ratownikiem    
    // Utworzenie kolejki dla ratownika
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    int ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);

    // Wysyłanie komunikatu do ratownika
    ratownik = klient.wybor_basenu;
    wyslany.mtype = ratownik;
    wyslany.PID = klient.PID;
    wyslany.wiek = klient.wiek;
    wyslany.wiek_opiekuna = klient.wiek_opiekuna;
	msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(wyslany) - sizeof(long), 0);

    // Klient odbiera wiadomość zwrotną od ratownika
    odebrany.PID = klient.PID;
	msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.PID, 0);    

    // Jeśli klient dostał się do basenu, pływa aż nadejdzie koniec czasu
    if(odebrany.pozwolenie == true)
    {
        pthread_join(czas, NULL);
    }
    else
    {
        // Ponowna próba z drugim wyborem
        // Klient ustawia takie wartości, aby wiadomość dotarła do ratownika
        ratownik = klient.drugi_wybor_basenu;
        wyslany.mtype = ratownik;
        wyslany.PID = klient.PID;
        wyslany.wiek = klient.wiek;
        wyslany.wiek_opiekuna = klient.wiek_opiekuna;
        msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(wyslany) - sizeof(long), 0);

        // Klient odbiera wiadomość zwrotną od ratownika
        // Ratownik odsyła wiadomość na PID klienta, dlatego aby odebrać wiadomość klient ustawia swoje PID
        odebrany.PID = klient.PID;
        msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.PID, 0);

        if(odebrany.pozwolenie == true)
        {
            pthread_join(czas, NULL);
        }
        else
        {
            raise(SIGINT); // Jeśli klient nie dostał się również do drugiego wyboru, kończy działanie
        }
    }

    // Komunikacja z ratownikiem, aby wyjść z basenu
    // Utworzenie kolejki dla ratownika wypuszczającego
    key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
    int ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600);
    // Klient ustawia takie wartości, aby wiadomość dotarła do ratownika
    wyslany.mtype = ratownik;
    wyslany.PID = klient.PID;
    wyslany.wiek = klient.wiek;
    wyslany.wiek_opiekuna = klient.wiek_opiekuna;
    // Klient wysyła wiadomość do ratownika
    msgsnd(ID_kolejki_ratownik_wypuszcza, &wyslany, sizeof(wyslany) - sizeof(long), 0);

    // Ratownik odsyła wiadomość na PID klienta, dlatego aby odebrać wiadomość klient ustawia ktype na swoje PID
    odebrany.PID = klient.PID;

    // Klient odbiera wiadomość zwrotną od ratownika
    msgrcv(ID_kolejki_ratownik_wypuszcza, &odebrany, sizeof(struct komunikat) - sizeof(long), odebrany.PID, 0);
    printf("%s", odebrany.mtext);

    // Gdy klient zakończy działanie wywołuje SIGINT
    raise(SIGINT);

	return 0;
}