#include "utils.c"

time_t czas_wyjscia;

// Funkcja do wysyłania komunikatów
void wyslij_komunikat(int kolejka_id, struct komunikat* kom)
{
    msgsnd(kolejka_id, kom, sizeof(*kom) - sizeof(long), 0);
}

// Funkcja do odbierania komunikatów
void odbierz_komunikat(int kolejka_id, struct komunikat* kom, long mtype)
{
    msgrcv(kolejka_id, kom, sizeof(*kom) - sizeof(long), mtype, 0);
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
    printf("%s[%s] Klient %d kończy działanie%s\n", COLOR2, timestamp(), getpid(), RESET);

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
    klient.wybor_basenu = 11 + rand() % 3; // Wybór basenu: 11 - brodzik, 12 - basen rekreacyjny, 13 - basen olimpijski
    klient.drugi_wybor_basenu = 11 + rand() % 3; // Drugi wybór basenu

    // Komunikat o uruchomieniu klienta
    if(klient.wiek_opiekuna == 0)
    {
        printf("%s[%s] Klient %d uruchomiony. Wiek: %d. VIP: %d. Czepek: %d. Preferowane baseny: %d, %d\n%s", COLOR2, timestamp(), klient.PID, klient.wiek, klient.VIP, klient.czepek, klient.wybor_basenu, klient.drugi_wybor_basenu, RESET);
    }
    else
    {
        printf("%s[%s] Klient %d uruchomiony. Wiek: %d. Wiek opiekuna: %d. VIP: %d. Czepek: %d. Preferowane baseny: %d, %d\n%s", COLOR2, timestamp(), klient.PID, klient.wiek, klient.wiek_opiekuna, klient.VIP, klient.czepek, klient.wybor_basenu, klient.drugi_wybor_basenu, RESET);
    }

    // Uzyskanie dostępu do segmentu pamięci dzielonej, która przechowuje zmienną bool czas_przekroczony
    key_t klucz_pamieci = ftok(".", 3213);
    int ID_pamieci = shmget(klucz_pamieci, sizeof(bool), 0600 | IPC_CREAT);
    bool* czas_przekroczony = (bool*)shmat(ID_pamieci, NULL, 0);
    
    // Deklaracja struktur do wysyłania i odbierania wiadomości od kasjera i ratownika
    struct komunikat wyslany, odebrany;

    // Komunikacja z kasjerem
    key_t klucz_kolejki_kasjer = ftok(".", 6377);
    int ID_kolejki_kasjer = msgget(klucz_kolejki_kasjer, IPC_CREAT | 0600); // Uzyskanie dostępu do kolejki kasjera

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
    pthread_create(&czas, NULL, czasomierz, NULL);

    // Komunikat o założeniu pampersa
    if(klient.wiek <= 3)
    {
        printf("%s[%s] Klientowi %d opiekun założył pampersa\n%s", COLOR2, timestamp(), klient.PID, RESET);
    }

    // Komunikat o założeniu czepka
    if(klient.czepek == true)
    {
        printf("%s[%s] Klient %d założył czepek\n%s", COLOR2, timestamp(), klient.PID, RESET);
    }

    // Komunikacja z ratownikiem
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    int ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600); // Utworzenie kolejki dla ratownika

    // Wysyłanie komunikatu do ratownika (pierwszy wybór basenu)
    inicjalizuj_komunikat(&wyslany, klient.wybor_basenu, klient.PID, klient.wiek, klient.wiek_opiekuna);
    wyslij_komunikat(ID_kolejki_ratownik_przyjmuje, &wyslany);

    // Klient odbiera wiadomość zwrotną od ratownika
    odbierz_komunikat(ID_kolejki_ratownik_przyjmuje, &odebrany, wyslany.PID);
    
    if(odebrany.pozwolenie == true) // Jeśli klient dostał się do basenu, pływa aż nadejdzie koniec czasu
    {
        pthread_join(czas, NULL);
    }
    else // Jeśli nie dostał się na basen, próbuje dostać się do swojego drugiego wyboru
    {
        inicjalizuj_komunikat(&wyslany, klient.drugi_wybor_basenu, klient.PID, klient.wiek, klient.wiek_opiekuna);
        wyslij_komunikat(ID_kolejki_ratownik_przyjmuje, &wyslany);

        // Klient odbiera wiadomość zwrotną od ratownika
        odbierz_komunikat(ID_kolejki_ratownik_przyjmuje, &odebrany, wyslany.PID);

        if(odebrany.pozwolenie == true) // Jeśli klient dostał się do basenu, pływa aż nadejdzie koniec czasu
        {
            pthread_join(czas, NULL);
        }
        else
        {
            raise(SIGINT); // Jeśli klient nie dostał się również do drugiego wyboru, kończy działanie
        }
    }

    // Komunikacja z ratownikiem, aby wyjść z basenu
    key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
    int ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600); // Utworzenie kolejki dla ratownika wypuszczającego

    // Klient wysyła wiadomość do ratownika
    wyslij_komunikat(ID_kolejki_ratownik_wypuszcza, &wyslany);

    // Klient odbiera wiadomość zwrotną od ratownika
    odbierz_komunikat(ID_kolejki_ratownik_wypuszcza, &odebrany, wyslany.PID);

    // Gdy klient chce wyjść z basenu, wywołuje SIGINT
    raise(SIGINT);

    return 0;
}