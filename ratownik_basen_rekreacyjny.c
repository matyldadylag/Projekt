#include "utils.c"


// ID struktur korzystanych przez wątki
int ID_kolejki_ratownik_przyjmuje, ID_kolejki_ratownik_wypuszcza, ID_semafora_rekreacyjny;
// Zmienna określająca czy trwa okresowe zamknięcie, pobierana z pamięci dzielonej
bool *okresowe_zamkniecie;
// Wątki
pthread_t przyjmuje, wypuszcza;
// Flaga sprawdzające, czy wydarzenie zostało już obsłużone
bool zamkniecie_handled = false;
// Zmienna z informacją, czy został wysłany sygnał SIGUSR1
bool sygnal;
// Tablica przechowująca PID klientów aktualnie na basenie
pid_t klienci_w_basenie[MAKS_REKREACYJNY];
// Licznik klientów aktualnie na basenie (liczy opiekuna i dziecko jako jeden)
int licznik_klientow = 0;
// Licznik klientów wiek (liczy opiekuna i dziecko osobno)
int licznik_klientow_wiek = 0;
// Zmienna przechowującą sumę wieku w basenie
double suma_wieku = 0;
// Muteks chroniący powyższe zasoby
pthread_mutex_t klient_mutex = PTHREAD_MUTEX_INITIALIZER;

void* przyjmowanie();
void* wypuszczanie();
void okresowe_zamkniecie_handler();
void SIGINT_handler(int sig);
void SIGUSR1_handler(int sig);
void SIGUSR2_handler(int sig);
void wyswietl_basen();

int main()
{
    // Komunikat o uruchomieniu ratownika basenu rekreacyjnego
    printf("%s[%s] Ratownik basenu rekreacyjnego uruchomiony%s\n", COLOR5, timestamp(), RESET);
   
    // Obsługa SIGINT
    if (signal(SIGINT, SIGINT_handler) == SIG_ERR)
    {
        handle_error("ratownik_rekreacyjny: signal SIGINT_handler");
    }

    /*// Obsługa sygnału SIGUSR1
    if (signal(SIGUSR1, SIGUSR1_handler) == SIG_ERR)
    {
        handle_error("ratownik_rekreacyjny: signal SIGUSR1_handler");
    }

    // Obsługa sygnału SIGUSR1
    if (signal(SIGUSR2, SIGUSR2_handler) == SIG_ERR)
    {
        handle_error("ratownik_rekreacyjny: signal SIGUSR2_handler");
    }*/

    // Uzyskanie dostępu do kolejki komunikatów dla ratowników - przyjmowanie klientów
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    if(klucz_kolejki_ratownik_przyjmuje==-1)
    {
        handle_error("ratownik_rekreacyjny: ftok klucz_kolejki_ratownik_przyjmuje");
    }
    ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_przyjmuje==-1)
    {
        handle_error("ratownik_rekreacyjny: msgget ID_kolejki_ratownik_przyjmuje");
    }
    
    // Uzyskanie dostępu do kolejki komunikatów dla ratowników - wypuszczanie klientów
    key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
    if(klucz_kolejki_ratownik_wypuszcza==-1)
    {
        handle_error("ratownik_rekreacyjny: ftok klucz_kolejki_ratownik_wypuszcza");
    }
    ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_wypuszcza==-1)
    {
        handle_error("ratownik_rekreacyjny: msgget ID_kolejki_ratownik_wypuszcza");
    }

    // Utworzenie semafora dla basenu rekreacyjnego
    key_t klucz_semafora_rekreacyjny = ftok(".", 2003);
    if(klucz_semafora_rekreacyjny == -1)
    {
        handle_error("ratownik_rekreacyjny: ftok klucz_semafora_rekreacyjny");
    }
    ID_semafora_rekreacyjny = semget(klucz_semafora_rekreacyjny, 1, 0600 | IPC_CREAT);
    if(ID_semafora_rekreacyjny == -1)
    {
        handle_error("ratownik_rekreacyjny: semget ID_semafora_rekreacyjny");
    }
    if(semctl(ID_semafora_rekreacyjny, 0, SETVAL, MAKS_REKREACYJNY) == -1)
    {
        handle_error("ratownik_rekreacyjny: semctl SETVAL ID_semafora_rekreacyjny");
    }

    // Uzyskanie dostępu do segmentu pamięci dzielonej, która przechowuje zmienną bool okresowe_zamkniecie
    key_t klucz_pamieci_okresowe_zamkniecie = ftok(".", 9929);
    if(klucz_pamieci_okresowe_zamkniecie==-1)
    {
        handle_error("ratownik_rekreacyjny: ftok klucz_pamieci_okresowe_zamkniecie");
    }
    int ID_pamieci_okresowe_zamkniecie = shmget(klucz_pamieci_okresowe_zamkniecie, sizeof(bool), 0600 | IPC_CREAT);
    if(ID_pamieci_okresowe_zamkniecie==-1)
    {
        handle_error("ratownik_rekreacyjny: shmget ID_pamieci_okresowe_zamkniecie");
    }
    okresowe_zamkniecie = (bool*)shmat(ID_pamieci_okresowe_zamkniecie, NULL, 0);
    if (okresowe_zamkniecie == (void*)-1)
    {
        handle_error("ratownik_rekreacyjny: shmat okresowe_zamkniecie");
    }

    sygnal = false;

    // Utworzenie wątków do przyjmowania i wypuszczania klientów
    if(pthread_create(&przyjmuje, NULL, przyjmowanie, NULL) != 0)
    {
        handle_error("ratownik_rekreacyjny: pthread_create przyjmuje");
    }
    if(pthread_create(&wypuszcza, NULL, wypuszczanie, NULL) != 0)
    {
        handle_error("ratownik_rekreacyjny: pthread_create wypuszcza");
    }

    // Dołączenie wątków do przyjmowania i wypuszczania klientów
    if(pthread_join(przyjmuje, NULL) != 0)
    {
        handle_error("ratownik_rekreacyjny: pthread_join przyjmuje");
    }
    if(pthread_join(wypuszcza, NULL) != 0)
    {
        handle_error("ratownik_rekreacyjny: pthread_join wypuszczanie");
    }

    return 0;
}

// Wątek przyjmujący klientów do basenu rekreacyjnego
void* przyjmowanie()
{
    // Deklaracja struktur do odbierania i wysyłania wiadomości od klientów
    struct komunikat odebrany, wyslany;
    // Deklaracja zmiennej pomocniczej do liczenia średniej
    double temp;

    while(1) // Dopóki ratownik nie dostanie SIGINT od zarządcy
    {
        // Jeśli trwa okresowe zamknięcie, wywołuje obsługującą funkcję
        if(*okresowe_zamkniecie)
        {
            okresowe_zamkniecie_handler();
            sleep(1);
            continue;
        }

        // Odebranie wiadomości
        if(msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_REKREACYJNY, 0) == -1)
        {
            handle_error("ratownik_rekreacyjny: msgrcv ID_kolejki_ratownik_przyjmuje");
        }
        
        // Obliczenie średniej, jaka była by w basenie w przypadku wejścia klienta
        if(odebrany.wiek_opiekuna==0) // Jeśli to klient bez dziecka
        {
            temp = (suma_wieku + odebrany.wiek)/(licznik_klientow_wiek+1);
        }
        else // Jeśli to klient z dzieckiem
        {
            temp = (suma_wieku + odebrany.wiek + odebrany.wiek_opiekuna)/(licznik_klientow_wiek+2);
        }

        // Decyzja o przyjęciu klienta
        if(temp <= 40 && *okresowe_zamkniecie == false &&  sygnal == false) // Sprawdzenie, czy nie trwa okresowe zamknięcie lub nie został wysłany sygnał
        {
            if(*okresowe_zamkniecie==true)
            {
                continue;
            }
            semafor_p(ID_semafora_rekreacyjny, 0); // Obniżenie semafora - jeśli nie ma aktualnie miejsca na basenie, czeka
            wyslany.pozwolenie = true; // Klient dostaje pozwolenie
            pthread_mutex_lock(&klient_mutex); // Blokada muteksu
            if(licznik_klientow>=MAKS_REKREACYJNY)
            {
                handle_error("ratownik_rekreacyjny: licznik_klientow poza zakresem");
            }
            klienci_w_basenie[licznik_klientow++] = odebrany.PID; // Dodanie klienta do tablicy i podwyższenie licznika klientów
            suma_wieku = suma_wieku + odebrany.wiek + odebrany.wiek_opiekuna; // Niezależnie od tego czy to opiekun z dzieckiem, czy klient bez dzieci
            licznik_klientow_wiek += (odebrany.wiek_opiekuna == 0) ? 1 : 2; // W zależności od tego czy to klient sam czy z dzieckiem, dodaje do licznika_wiek 1 lub 2
            pthread_mutex_unlock(&klient_mutex); // Odblokowanie muteksu
        }
        else
        {
            wyslany.pozwolenie = false;
        }

        // Wysłanie wiadomości
        wyslany.mtype = odebrany.PID;

        if(msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
        {
            handle_error("ratownik_rekreacyjny: msgsnd ID_kolejki_ratownik_przyjmuje");
        }

        // Komunikat o przyjęciu lub nie klienta (w zależności czy to klient z dzieckiem czy nie)
        if(wyslany.pozwolenie == true)
        {
            if(odebrany.wiek_opiekuna==0)
            {
                printf("%s[%s] Ratownik basenu rekreacyjnego przyjął klienta %d%s\n", COLOR5, timestamp(), odebrany.PID, RESET);
        
            }
            else
            {
                printf("%s[%s] Ratownik basenu rekreacyjnego przyjął klienta %d z opiekunem%s\n", COLOR5, timestamp(), odebrany.PID, RESET);
            }
        }
        else
        {
            printf("%s[%s] Ratownik basenu rekreacyjnego nie przyjął klienta %d%s\n", COLOR5, timestamp(), odebrany.PID, RESET);
        }

        // Wyświetlenie aktualnego stanu basenu
        wyswietl_basen();
    }
}

// Wątek wypuszczający klientów z basenu rekreacyjnego
void* wypuszczanie()
{
    // Deklaracja struktur wysyłanych i odbieranych od klienta
    struct komunikat odebrany, wyslany;

    while(1) // Dopóki ratownik nie dostanie SIGINT od zarządcy
    {
        // Odebranie wiadomości
        if(msgrcv(ID_kolejki_ratownik_wypuszcza, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_REKREACYJNY, 0) == -1)
        {
            handle_error("ratownik_rekreacyjny: msgrcv ID_kolejki_ratownik_wypuszcza");
        }

        semafor_v(ID_semafora_rekreacyjny, 0);
        
        // Usunięcie klienta wychodzącego z tablicy
        pthread_mutex_lock(&klient_mutex); // Blokada muteksu
        int indeks_klienta_do_usuniecia;
        for (int i = 0; i < licznik_klientow; i++) // Znaleznie indeksu pod którym znajduje się PID usuwanego klienta
        {
            if (klienci_w_basenie[i] == odebrany.PID)
            {
                indeks_klienta_do_usuniecia = i;
                break;
            }
        }
        for (int i = indeks_klienta_do_usuniecia; i < licznik_klientow - 1; i++) // Przesunięcie pozostałych klientów w tablicy
        {
            klienci_w_basenie[i] = klienci_w_basenie[i + 1];
        }
        licznik_klientow--; // Obniżenie licznika klientów
        suma_wieku = suma_wieku - odebrany.wiek - odebrany.wiek_opiekuna; // Niezależnie od tego czy to opiekun z dzieckiem, czy klient bez dzieci
        licznik_klientow_wiek -= (odebrany.wiek_opiekuna == 0) ? 1 : 2; // W zależności od tego czy to klient sam czy z dzieckiem, odejmuje do licznika_wiek 1 lub 2
        pthread_mutex_unlock(&klient_mutex); // Odblokowanie muteksu

        // Wysłanie wiadomości
        wyslany.mtype = odebrany.PID;
        if(msgsnd(ID_kolejki_ratownik_wypuszcza, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
        {
            handle_error("ratownik_rekreacyjny: msgsnd ID_kolejki_ratownik_wypuszcza");
        }

        // Komunikat o wypuszczeniu klienta (w zależności czy klient jest sam czy z dzieckiem)
        if(odebrany.wiek_opiekuna==0)
        {
            printf("%s[%s] Ratownik basenu rekreacyjnego wypuścił klienta %d%s\n", COLOR5, timestamp(), odebrany.PID, RESET);
        }
        else
        {
            printf("%s[%s] Ratownik brodzika wypuścił klienta %d z opiekunem%s\n", COLOR5, timestamp(), odebrany.PID, RESET);
        }
        
        // Wyświetlenie aktualnego stanu basenu
        wyswietl_basen();
    }
}

// Obsługa okresowego zamknięcia
void okresowe_zamkniecie_handler()
{
    if (!zamkniecie_handled) // Sprawdza, czy okresowe zamknięcie nie zostało już obsłużone
    {
        pthread_mutex_lock(&klient_mutex); // Blokada muteksu
        for (int i = 0; i < licznik_klientow; i++) // Usuwa wszystkie PID w tablicy
        {
            klienci_w_basenie[i] = 0;
        }
        licznik_klientow = 0; // Resetuje licznik klientów
        licznik_klientow_wiek = 0;
        suma_wieku = 0;
        pthread_mutex_unlock(&klient_mutex);
        semctl(ID_semafora_rekreacyjny, 0, SETVAL, MAKS_REKREACYJNY); // Ustawia wartość semafora tak jakby nikogo nie było w basenie
        zamkniecie_handled = true; // Oznacza okresowe zamknięcie jako obsłużone
        printf("%s[%s] Ratownik basenu rekreacyjnego wyprosił wszystkich klientów%s\n", COLOR5, timestamp(), RESET);
        wyswietl_basen();
    }
}

// Obsługa SIGINT
void SIGINT_handler(int sig)
{
    // Anulowanie wątków przyjmowania i wypuszczania klientów
    pthread_kill(przyjmuje, SIGINT);
    pthread_kill(wypuszcza, SIGINT);

    // Usunięcie muteksu
    if (pthread_mutex_destroy(&klient_mutex) != 0)
    {
        handle_error("ratownik_brodzik: pthread_mutex_destroy klient_mutex");
    }

    // Komunikat o zakończeniu działania ratownika basenu rekreacyjnego
    printf("%s[%s] Ratownik basenu rekreacyjnego kończy działanie%s\n", COLOR5, timestamp(), RESET);

    exit(0);
}

void wyswietl_basen()
{
    printf("%s[%s] Basen rekreacyjny: [", COLOR5, timestamp());
    for (int i = 0; i < licznik_klientow; i++)
    {
        printf("%d", klienci_w_basenie[i]);
        if (i < licznik_klientow - 1)
        {
            printf(", ");
        }
    }
    printf("]");
    if (licznik_klientow_wiek > 0)
    {
        printf(" Średnia wieku: %.2lf%s\n", suma_wieku / licznik_klientow_wiek, RESET);
    }
    else
    {
        printf(" Średnia wieku: brak danych%s\n", RESET);
    }
}