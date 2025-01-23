#include "utils.c"

time_t czas_wyjscia;

// Funkcja do wysyłania komunikatów
void wyslij_komunikat(int kolejka_id, struct komunikat* kom)
{
    if(msgsnd(kolejka_id, kom, sizeof(*kom) - sizeof(long), 0)==-1)
    {
        handle_error("msgsnd wyslij_komunikat");
    }
}

// Funkcja do odbierania komunikatów
void odbierz_komunikat(int kolejka_id, struct komunikat* kom, long mtype)
{
    if(msgrcv(kolejka_id, kom, sizeof(*kom) - sizeof(long), mtype, 0)==-1)
    {
        handle_error("msgrcv odbierz_komunikat");
    }
}

// Funkcja do inicjalizacji komunikatu
void inicjalizuj_komunikat(struct komunikat* kom, long mtype, int PID, int wiek, int wiek_opiekuna)
{
    kom->mtype = mtype;
    kom->PID = PID;
    kom->wiek = wiek;
    kom->wiek_opiekuna = wiek_opiekuna;
}

void SIGINT_handler(int sig)
{
    // Komunikat o zakończeniu działania klienta
    printf("%s[%s] Klient %d kończy działanie%s\n", COLOR3, timestamp(), getpid(), RESET);

    exit(0);
}

// Wątek mierzy czas - gdy zostanie osiągnięty czas wyjścia klienta z basenu, wysyła komunikat do ratownika o chęci wyjścia
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
    if (signal(SIGINT, SIGINT_handler) == SIG_ERR)
    {
        handle_error("signal SIGINT_handler");
    }

    // Punkt początkowy do losowania na podstawie PID procesu
    srand(getpid());

    // Utworzenie semafora
    key_t klucz_semafora_brodzik = ftok(".", 3293);
    if(klucz_semafora_brodzik == -1)
    {
        handle_error("ftok klucz_semafora");
    }
    int ID_semafora_brodzik = semget(klucz_semafora_brodzik, 1, 0600 | IPC_CREAT);
    if(ID_semafora_brodzik == -1)
    {
        handle_error("semget ID_semafora");
    }
    if(semctl(ID_semafora_brodzik, 0, SETVAL, MAKS_BRODZIK) == -1)
    {
        handle_error("semctl SETVAL");
    }
  

 // Utworzenie semafora
    key_t klucz_semafora_rekreacyjny = ftok(".", 2003);
    if(klucz_semafora_rekreacyjny == -1)
    {
        handle_error("ftok klucz_semafora");
    }
    int ID_semafora_rekreacyjny = semget(klucz_semafora_rekreacyjny, 1, 0600 | IPC_CREAT);
    if(ID_semafora_rekreacyjny == -1)
    {
        handle_error("semget ID_semafora");
    }
    if(semctl(ID_semafora_rekreacyjny, 0, SETVAL, MAKS_REKREACYJNY) == -1)
    {
        handle_error("semctl SETVAL");
    }

    // Utworzenie semafora
    key_t klucz_semafora_olimpijski = ftok(".", 9447);
    if(klucz_semafora_olimpijski==-1)
    {
        handle_error("ftok klucz_semafora");
    }
    int ID_semafora_olimpijski = semget(klucz_semafora_olimpijski, 1, 0600|IPC_CREAT);
    if(ID_semafora_olimpijski==-1)
    {
        handle_error("semget ID_semafora");
    }
    if(semctl(ID_semafora_olimpijski, 0, SETVAL, MAKS_OLIMPIJSKI)==-1)
    {
        handle_error("semctl ID_semafora");
    }


    // Zainicjowanie danych klienta
    struct dane_klienta klient;
    klient.PID = getpid();
    klient.wiek = (rand() % 70) + 1; // Losowo generuje wiek od 1 do 70 lat
    klient.wiek_opiekuna = (klient.wiek <= 10) ? ((rand() % 53) + 19) : 0; // W wypadku, gdy klient.wiek <= 10 generuje się wiek opiekuna (między 18 a 70 lat)
    klient.VIP = (rand() % 5 == 0); // Klient ma szansę 1:5 na bycie VIP
    klient.czepek = (rand() % 5 == 0); // Klient ma szansę 1:5 na założenie czepka
    klient.wybor_basenu = 11 + rand() % 3; // Wybór basenu: 11 - brodzik, 12 - basen rekreacyjny, 13 - basen olimpijski
    klient.drugi_wybor_basenu = 11 + rand() % 3; // Drugi wybór basenu

    // Komunikat o uruchomieniu klienta
    if(klient.wiek_opiekuna == 0)
    {
        printf("%s[%s] Klient %d uruchomiony. Wiek: %d. VIP: %d. Czepek: %d. Preferowane baseny: %d, %d\n%s", COLOR3, timestamp(), klient.PID, klient.wiek, klient.VIP, klient.czepek, klient.wybor_basenu, klient.drugi_wybor_basenu, RESET);
    }
    else
    {
        printf("%s[%s] Klient %d uruchomiony. Wiek: %d. Wiek opiekuna: %d. VIP: %d. Czepek: %d. Preferowane baseny: %d, %d\n%s", COLOR3, timestamp(), klient.PID, klient.wiek, klient.wiek_opiekuna, klient.VIP, klient.czepek, klient.wybor_basenu, klient.drugi_wybor_basenu, RESET);
    }

    // Uzyskanie dostępu do segmentu pamięci dzielonej, która przechowuje zmienną bool czas_przekroczony
    key_t klucz_pamieci = ftok(".", 3213);
    if (klucz_pamieci == -1)
    {
        handle_error("ftok klucz_pamieci");
    }

    int ID_pamieci = shmget(klucz_pamieci, sizeof(bool), 0600 | IPC_CREAT);
    if (ID_pamieci == -1)
    {
        handle_error("shmget ID_pamieci");
    }

    bool* czas_przekroczony = (bool*)shmat(ID_pamieci, NULL, 0);
    if (czas_przekroczony == (void*)-1)
    {
        handle_error("shmat czas_przekroczony");
    }

    // Deklaracja struktur do wysyłania i odbierania wiadomości od kasjera i ratownika
    struct komunikat wyslany, odebrany;

    // Komunikacja z kasjerem
    key_t klucz_kolejki_kasjer = ftok(".", 6377);
    if (klucz_kolejki_kasjer == -1)
    {
        handle_error("ftok klucz_kolejki_kasjer");
    }

    int ID_kolejki_kasjer = msgget(klucz_kolejki_kasjer, IPC_CREAT | 0600);
    if (ID_kolejki_kasjer == -1)
    {
        handle_error("msgget ID_kolejki_kasjer");
    }

    // Inicjalizacja struktury komunikatu dla kasjera
    inicjalizuj_komunikat(&wyslany, klient.VIP ? KASJER_VIP : KASJER, klient.PID, klient.wiek, klient.wiek_opiekuna);

    // Wysyłanie komunikatu do kasjera
    wyslij_komunikat(ID_kolejki_kasjer, &wyslany);

    // Odbieranie komunikatu od kasjera
    odbierz_komunikat(ID_kolejki_kasjer, &odebrany, wyslany.PID);

    // Klient wchodzi na basen, czas z biletu zaczyna upływać
    czas_wyjscia = time(NULL) + BILET;

    // Tworzy wątek pilnujący, kiedy kończy się bilet
    pthread_t czas;
    if (pthread_create(&czas, NULL, czasomierz, NULL) != 0)
    {
        handle_error("pthread_create czas");
    }

    // Komunikat o założeniu pampersa
    if(klient.wiek <= 3)
    {
        printf("%s[%s] Klientowi %d opiekun założył pampersa\n%s", COLOR3, timestamp(), klient.PID, RESET);
    }

    // Komunikat o założeniu czepka
    if(klient.czepek == true)
    {
        printf("%s[%s] Klient %d założył czepek\n%s", COLOR3, timestamp(), klient.PID, RESET);
    }

    // Komunikacja z ratownikiem
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    if (klucz_kolejki_ratownik_przyjmuje == -1)
    {
        handle_error("ftok klucz_kolejki_ratownik_przyjmuje");
    }

    int ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);
    if (ID_kolejki_ratownik_przyjmuje == -1)
    {
        handle_error("msgget ID_kolejki_ratownik_przyjmuje");
    }

    // Wysyłanie komunikatu do ratownika (pierwszy wybór basenu)
    inicjalizuj_komunikat(&wyslany, klient.wybor_basenu, klient.PID, klient.wiek, klient.wiek_opiekuna);
    wyslij_komunikat(ID_kolejki_ratownik_przyjmuje, &wyslany);

    // Klient odbiera wiadomość zwrotną od ratownika
    odbierz_komunikat(ID_kolejki_ratownik_przyjmuje, &odebrany, wyslany.PID);

    if(odebrany.pozwolenie == true) // Jeśli klient dostał się do basenu, pływa aż nadejdzie koniec czasu
    {
        printf("Klient %d opuszcza semafor %d\n", getpid(), odebrany.id_semafora);
        semafor_p(odebrany.id_semafora, 0);

        if (pthread_join(czas, NULL) != 0)
        {
            handle_error("pthread_join czas");
        }
    }
    else // Jeśli nie dostał się na basen, próbuje dostać się do swojego drugiego wyboru
    {
        inicjalizuj_komunikat(&wyslany, klient.drugi_wybor_basenu, klient.PID, klient.wiek, klient.wiek_opiekuna);
        wyslij_komunikat(ID_kolejki_ratownik_przyjmuje, &wyslany);

        // Klient odbiera wiadomość zwrotną od ratownika
        odbierz_komunikat(ID_kolejki_ratownik_przyjmuje, &odebrany, wyslany.PID);

        if(odebrany.pozwolenie == true) // Jeśli klient dostał się do basenu, pływa aż nadejdzie koniec czasu
        {
            semafor_p(odebrany.id_semafora, 0);

            if (pthread_join(czas, NULL) != 0)
            {
                handle_error("pthread_join czas");
            }
        }
        else
        {
            raise(SIGINT); // Jeśli klient nie dostał się również do drugiego wyboru, kończy działanie
        }
    }

    // Komunikacja z ratownikiem, aby wyjść z basenu
    key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
    if (klucz_kolejki_ratownik_wypuszcza == -1)
    {
        handle_error("ftok klucz_kolejki_ratownik_wypuszcza");
    }

    int ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600);
    if (ID_kolejki_ratownik_wypuszcza == -1)
    {
        handle_error("msgget ID_kolejki_ratownik_wypuszcza");
    }

    // Klient wysyła wiadomość do ratownika
    wyslij_komunikat(ID_kolejki_ratownik_wypuszcza, &wyslany);

    // Klient odbiera wiadomość zwrotną od ratownika
    odbierz_komunikat(ID_kolejki_ratownik_wypuszcza, &odebrany, wyslany.PID);

    printf("Klient %d zwalnia semafor %d\n", getpid(), odebrany.id_semafora);
    semafor_v(odebrany.id_semafora, 0);

    // Gdy klient chce wyjść z basenu, wywołuje SIGINT
    raise(SIGINT);

    return 0;
}