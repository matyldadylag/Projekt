#include "utils.c"

time_t czas_wyjscia; // O której klient ma wyjść z basenu
bool plywa; // Flaga, czy klient pływa
int wybor_basenu, basen_zakazany; // Zmiennie z aktualnym wyborem basenu klienta i zakazanym basenem przez SIGUSR1
// Flaga, czy trwa okresowe zamknięcie, pobierana z pamięci dzielonej
bool *okresowe_zamkniecie;

void SIGINT_handler(int sig);
void SIGUSR1_handler(int sig);
void SIGUSR2_handler(int sig);

int main()
{
    // Punkt początkowy do losowania na podstawie PID procesu
    srand(getpid());

    // Zainicjowanie danych klienta
    struct dane_klienta klient;
    klient.PID = getpid();
    klient.wiek = (rand() % 70) + 1; // Losowo generuje wiek od 1 do 70 lat
    klient.wiek_opiekuna = (klient.wiek <= 10) ? ((rand() % 53) + 19) : 0; // W wypadku, gdy klient.wiek <= 10 generuje się wiek opiekuna (między 18 a 70 lat)
    klient.VIP = (rand() % 5 == 0); // Klient ma szansę 1:5 na bycie VIP
    klient.czepek = (rand() % 5 == 0); // Klient ma szansę 1:5 na założenie czepka

    // Komunikat o uruchomieniu klienta
    if(klient.wiek_opiekuna == 0)
    {
        printf("%s[%s] Klient %d uruchomiony. Wiek: %d. VIP: %d. Czepek: %d%s\n", COLOR3, timestamp(), klient.PID, klient.wiek, klient.VIP, klient.czepek, RESET);
    }
    else
    {
        printf("%s[%s] Klient %d uruchomiony. Wiek: %d. Wiek opiekuna: %d. VIP: %d. Czepek: %d%s\n", COLOR3, timestamp(), klient.PID, klient.wiek, klient.wiek_opiekuna, klient.VIP, klient.czepek, RESET);
    }

    // Obsługa sygnału SIGINT
    if (signal(SIGINT, SIGINT_handler) == SIG_ERR)
    {
        handle_error("klient: signal SIGINT_handler");
    }

    // Obsługa sygnału SIGUSR1
    if (signal(SIGUSR1, SIGUSR1_handler) == SIG_ERR)
    {
        handle_error("klient: signal SIGUSR1_handler");
    }

    // Obsługa sygnału SIGUSR2
    if (signal(SIGUSR2, SIGUSR2_handler) == SIG_ERR)
    {
        handle_error("klient: signal SIGUSR2_handler");
    }

    // Utworzenie kolejki komunikatów dla kasjera
    key_t klucz_kolejki_kasjer = ftok(".", 6377);
    if (klucz_kolejki_kasjer == -1)
    {
        handle_error("klient: ftok klucz_kolejki_kasjer");
    }
    int ID_kolejki_kasjer = msgget(klucz_kolejki_kasjer, IPC_CREAT | 0600);
    if (ID_kolejki_kasjer == -1)
    {
        handle_error("klient: msgget ID_kolejki_kasjer");
    }

    // Utworzenie kolejki komunikatów dla ratowników - przyjmowanie klientów
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    if(klucz_kolejki_ratownik_przyjmuje==-1)
    {
        handle_error("klient: ftok klucz_kolejki_ratownik_przyjmuje");
    }
    int ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_przyjmuje==-1)
    {
        handle_error("klient: msgget ID_kolejki_ratownik_przyjmuje");
    }
    
    // Utworzenie kolejki komunikatów dla ratowników - wypuszczanie klientów
    key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
    if(klucz_kolejki_ratownik_wypuszcza==-1)
    {
        handle_error("klient: ftok klucz_kolejki_ratownik_wypuszcza");
    }
    int ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_wypuszcza==-1)
    {
        handle_error("klient: msgget ID_kolejki_ratownik_wypuszcza");
    }

    // Uzyskanie dostępu do segmentu pamięci dzielonej, która przechowuje zmienną bool okresowe_zamkniecie
    key_t klucz_pamieci_okresowe_zamkniecie = ftok(".", 9929);
    if(klucz_pamieci_okresowe_zamkniecie==-1)
    {
        handle_error("klient: ftok klucz_pamieci_okresowe_zamkniecie");
    }
    int ID_pamieci_okresowe_zamkniecie = shmget(klucz_pamieci_okresowe_zamkniecie, sizeof(bool), 0600 | IPC_CREAT);
    if(ID_pamieci_okresowe_zamkniecie==-1)
    {
        handle_error("klient: shmget ID_pamieci_okresowe_zamkniecie");
    }
    okresowe_zamkniecie = (bool*)shmat(ID_pamieci_okresowe_zamkniecie, NULL, 0);
    if (okresowe_zamkniecie == (void*)-1)
    {
        handle_error("klient: shmat okresowe_zamkniecie");
    }

    // Deklaracja struktur do wysyłania i odbierania wiadomości od kasjera i ratownika
    struct komunikat wyslany, odebrany;

    // Komunikacja z kasjerem
    // Inicjalizacja struktury wyslany
    wyslany.mtype = klient.VIP ? KASJER_VIP : KASJER;
    wyslany.PID = klient.PID;
    wyslany.wiek = klient.wiek;
    wyslany.wiek_opiekuna = klient.wiek_opiekuna;

    // Wysłanie wiadomości
    if(msgsnd(ID_kolejki_kasjer, &wyslany, sizeof(struct komunikat) - sizeof(long), 0)==-1)
    {
        handle_error("klient: msgsnd ID_kolejki_kasjer");
    }

    // Odebranie odpowiedzi zwrotnej
    if(msgrcv(ID_kolejki_kasjer, &odebrany, sizeof(struct komunikat) - sizeof(long), wyslany.PID, 0)==-1)
    {
        handle_error("klient: msgrcv ID_kolejki_kasjer");
    }

    if(odebrany.pozwolenie == true) // Jeśli klient dostał pozwolenie na wejście od kasjera
    {
        // Klient wchodzi na basen, czas z biletu zaczyna upływać
        czas_wyjscia = time(NULL) + BILET;

        // Komunikat o założeniu pampersa
        if(klient.wiek <= 3)
        {
            printf("%s[%s] Klientowi %d opiekun założył pampersa%s\n", COLOR3, timestamp(), klient.PID, RESET);
        }

        // Komunikat o założeniu czepka
        if(klient.czepek == true)
        {
            printf("%s[%s] Klient %d założył czepek%s\n", COLOR3, timestamp(), klient.PID, RESET);
        }

        // Komunikacja z ratownikiem
        // Inicjalizacja struktury wyslany
        wyslany.PID = klient.PID;
        wyslany.wiek = klient.wiek;
        wyslany.wiek_opiekuna = klient.wiek_opiekuna;

        // Zmienna śledząca, czy klient znajduje się aktualnie w basenie
        plywa = false;

        while(time(NULL)<czas_wyjscia)
        {
            if(*okresowe_zamkniecie==true)
            {
                plywa = false;
                sleep(1);
                continue;
            }
            if(plywa == false)
            {
                // Losowe wybranie basenu
                do {
                    wybor_basenu = 11 + rand() % 3;
                } while (wybor_basenu == basen_zakazany); // Losuje, dopóki nie wylosuje innego, niż ten, którym nie jest dozwolony

                // Wysłanie wiadomości
                wyslany.mtype = wybor_basenu;
                if(msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0)==-1)
                {
                    handle_error("klient: msgsnd ID_kolejki_ratownik_przyjmuje");
                }

                // Odebranie odpowiedzi zwrotnej
                if(msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(struct komunikat) - sizeof(long), wyslany.PID, 0)==-1)
                {
                    handle_error("klient: msgrcv ID_kolejki_ratownik_przyjmuje");
                }

                if(odebrany.pozwolenie == true) // Jeśli klient dostał się do basenu, pływa aż nadejdzie koniec czasu
                {
                    plywa = true;
                }
                else // Jeśli nie, czeka chwilę, a następnie próbuje ponownie
                {
                    int przerwa=rand()%BILET; // Losuje jak długo ma trwać przerwa
                    while(przerwa>0) // Dopóki nie minie czas przerwy idzie spać na sekundę, chyba że w między czasie nadejdzie czas wyjścia
                    {
                        if(time(NULL)>=czas_wyjscia)
                        {
                            break;
                        }
                        sleep(1);
                        przerwa--;
                    }
                }
            }
            else // Jeśli klient już pływa, kontynuje swoją aktywność
            {
                sleep(1);
            }
        }

        // Komunikacja z ratownikiem - prośba o wyjście, jeśli klient w momencie wyjścia z pętli był w jakimś basenie
        if(plywa == true)
        {
            // Wysłanie wiadomości z prośbą o wyjście
            if(msgsnd(ID_kolejki_ratownik_wypuszcza, &wyslany, sizeof(struct komunikat) - sizeof(long), 0)==-1)
            {
                handle_error("klient: msgsnd ID_kolejki_ratownik_wypuszcza");
            }

            // Odebranie odpowiedzi zwrotnej
            if(msgrcv(ID_kolejki_ratownik_wypuszcza, &odebrany, sizeof(struct komunikat) - sizeof(long), wyslany.PID, 0)==-1)
            {
                handle_error("klient: msgrcv ID_kolejki_ratownik_wypuszcza");
            }
        }
    }
    
    // Klient kończy działanie
    raise(SIGINT);

    return 0;
}

void SIGINT_handler(int sig)
{
    // Komunikat o zakończeniu działania klienta
    printf("%s[%s] Klient %d kończy działanie%s\n", COLOR3, timestamp(), getpid(), RESET);

    exit(0);
}

void SIGUSR1_handler(int sig)
{
    plywa = false;
    basen_zakazany = wybor_basenu;
}

void SIGUSR2_handler(int sig)
{
    basen_zakazany = 0;
}