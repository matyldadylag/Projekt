#include "utils.c"

// Funkcje dla wątków
void* przyjmowanie();
void* wypuszczanie();

// Zdefiniowane wyżej, aby być w stanie usunąć struktury asynchronicznie w przypadku SIGINT
int ID_kolejki_ratownik_przyjmuje;
int ID_kolejki_ratownik_wypuszcza;
int ID_semafora_rekreacyjny;
pthread_t przyjmuje, wypuszcza;
double suma_wieku;

// Tablica przechowująca PID klientów aktualnie na basenie
pid_t klienci_w_basenie[MAKS_REKREACYJNY];
// Licznik klientów aktualnie na basenie (liczy opiekuna i dziecko jako jeden)
int licznik_klientow = 0;
// Licznik klientów wiek
int licznik_klientow_wiek = 0;
// Muteks chroniący powyższe zasoby
pthread_mutex_t klient_mutex = PTHREAD_MUTEX_INITIALIZER;

// Obsługa SIGINT
void SIGINT_handler(int sig)
{
    // Anulowanie wątków przyjmowania i wypuszczania klientów
    pthread_kill(przyjmuje, SIGINT);
    pthread_kill(wypuszcza, SIGINT);

    // Wysłanie SIGINT do klientów z tablicy ratownika
    for (int i = 0; i < licznik_klientow; i++)
    {
        kill(klienci_w_basenie[i], SIGINT);
    }

    // Komunikat o zakończeniu działania ratownika basenu rekreacyjnego
    printf("%s[%s] Ratownik basenu rekreacyjnego kończy działanie%s\n", COLOR5, timestamp(), RESET);

    exit(0);
}

void print_klienci_w_basenie()
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
    printf("]%s\n", RESET);
}

int main()
{
    // Obsługa SIGINT
    if (signal(SIGINT, SIGINT_handler) == SIG_ERR)
    {
        handle_error("signal SIGINT_handler");
    }

    // Komunikat o uruchomieniu ratownika basenu rekreacyjnego
    printf("%s[%s] Ratownik basenu rekreacyjnego uruchomiony%s\n", COLOR5, timestamp(), RESET);
    
    // Utworzenie kolejki do przyjmowania klientów
    key_t klucz_kolejki_ratownik_przyjmuje = ftok(".", 7942);
    if(klucz_kolejki_ratownik_przyjmuje == -1)
    {
        handle_error("ftok klucz_kolejki_ratownik_przyjmuje");
    }
    ID_kolejki_ratownik_przyjmuje = msgget(klucz_kolejki_ratownik_przyjmuje, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_przyjmuje == -1)
    {
        handle_error("msgget ID_kolejki_ratownik_przyjmuje");
    }

    // Utworzenie kolejki do wypuszczania klientów
    key_t klucz_kolejki_ratownik_wypuszcza = ftok(".", 4824);
    if(klucz_kolejki_ratownik_wypuszcza == -1)
    {
        handle_error("ftok klucz_kolejki_ratownik_wypuszcza");
    }
    ID_kolejki_ratownik_wypuszcza = msgget(klucz_kolejki_ratownik_wypuszcza, IPC_CREAT | 0600);
    if(ID_kolejki_ratownik_wypuszcza == -1)
    {
        handle_error("msgget ID_kolejki_ratownik_wypuszcza");
    }

 // Utworzenie semafora
    key_t klucz_semafora_rekreacyjny = ftok(".", 2003);
    if(klucz_semafora_rekreacyjny == -1)
    {
        handle_error("ftok klucz_semafora");
    }
    ID_semafora_rekreacyjny = semget(klucz_semafora_rekreacyjny, 1, 0600 | IPC_CREAT);
    if(ID_semafora_rekreacyjny == -1)
    {
        handle_error("semget ID_semafora");
    }
    if(semctl(ID_semafora_rekreacyjny, 0, SETVAL, MAKS_REKREACYJNY) == -1)
    {
        handle_error("semctl SETVAL");
    }

    // Utworzenie wątków do przyjmowania i wypuszczania klientów
    if(pthread_create(&przyjmuje, NULL, przyjmowanie, NULL) != 0)
    {
        handle_error("pthread_create przyjmowanie");
    }
    if(pthread_create(&wypuszcza, NULL, wypuszczanie, NULL) != 0)
    {
        handle_error("pthread_create wypuszczanie");
    }

    if(pthread_join(przyjmuje, NULL) != 0)
    {
        handle_error("pthread_join przyjmowanie");
    }
    if(pthread_join(wypuszcza, NULL) != 0)
    {
        handle_error("pthread_join wypuszczanie");
    }

    return 0;
}

void* przyjmowanie()
{
    // Deklaracja struktur do odbierania i wysyłania wiadomości od klientów
    struct komunikat odebrany, wyslany;
    double temp;

    while(1)
    {
        print_klienci_w_basenie();

        if(msgrcv(ID_kolejki_ratownik_przyjmuje, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_REKREACYJNY, 0) == -1)
        {
            handle_error("msgrcv ratownik rekreacyjny przyjmuje");
        }
        
        if(odebrany.wiek_opiekuna==0)
        {
            // Wykorzystuję zmienną temp, bo jeszcze nie wiem czy klienta przyjmę - sprawdzam czy średnia będzie za wysoka
            temp = (suma_wieku + odebrany.wiek)/(licznik_klientow_wiek+1);
        
            if(temp <= 40)
            {
                // Przyjęcie klienta na basen
                suma_wieku += odebrany.wiek;
            
                // Dodanie PID klienta do tablicy
                pthread_mutex_lock(&klient_mutex);
                klienci_w_basenie[licznik_klientow++] = odebrany.PID;
                pthread_mutex_unlock(&klient_mutex);
                licznik_klientow_wiek++;

                wyslany.mtype = odebrany.PID;
                wyslany.PID = odebrany.PID;
                wyslany.pozwolenie = true;
                wyslany.ID_semafora = ID_semafora_rekreacyjny;

                if(msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
                {
                    handle_error("msgsnd rekreacyjny ratownik przyjmuje");
                }

                printf("%s[%s] Ratownik basenu rekreacyjnego przyjął klienta %d%s\n", COLOR5, timestamp(), odebrany.PID, RESET);
            }
            else
            {
                wyslany.mtype = odebrany.PID;
                wyslany.PID = odebrany.PID;
                wyslany.pozwolenie = false;

                if(msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
                {
                    handle_error("msgsnd ratownik rekreacyjny przyjmuje");
                }

                printf("%s[%s] Ratownik basenu rekreacyjnego nie przyjął klienta %d%s\n", COLOR5, timestamp(), odebrany.PID, RESET);
            }
        }
        else
        {
            // Wykorzystuję zmienną temp, bo jeszcze nie wiem czy klienta przyjmę - sprawdzam czy średnia będzie za wysoka
            temp = (suma_wieku + odebrany.wiek + odebrany.wiek_opiekuna)/(licznik_klientow_wiek+2);
        
            if(temp <= 40)
            {
                // Przyjęcie klienta na basen
                suma_wieku = suma_wieku + odebrany.wiek + odebrany.wiek_opiekuna;
            
                // Dodanie PID klienta do tablicy
                pthread_mutex_lock(&klient_mutex);
                klienci_w_basenie[licznik_klientow++] = odebrany.PID;
                pthread_mutex_unlock(&klient_mutex);

                licznik_klientow_wiek = licznik_klientow_wiek + 2;

                wyslany.mtype = odebrany.PID;
                wyslany.PID = odebrany.PID;
                wyslany.pozwolenie = true;
                wyslany.ID_semafora = ID_semafora_rekreacyjny;

                printf("Ratownik rekreacyjny wysyła strukturę:\nmtype:%lf\nPID:%d\nwiek:%d\nwiek_opiekuna:%d\npozwolenie:%d\nID_semafora:%d\n", wyslany.mtype, wyslany.PID, wyslany.wiek, wyslany.wiek_opiekuna, wyslany.pozwolenie, wyslany.ID_semafora);

                if(msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
                {
                    handle_error("msgsnd ratownik rekreacyjny przyjmuje");
                }

                printf("%s[%s] Ratownik basenu rekreacyjnego przyjął klienta %d z opiekunem%s\n", COLOR5, timestamp(), odebrany.PID, RESET);
            }
            else
            {
                wyslany.mtype = odebrany.PID;
                wyslany.PID = odebrany.PID;
                wyslany.pozwolenie = false;

                printf("Ratownik rekreacyjny wysyła strukturę:\nmtype:%lf\nPID:%d\nwiek:%d\nwiek_opiekuna:%d\npozwolenie:%d\nID_semafora:%d\n", wyslany.mtype, wyslany.PID, wyslany.wiek, wyslany.wiek_opiekuna, wyslany.pozwolenie, wyslany.ID_semafora);

                if(msgsnd(ID_kolejki_ratownik_przyjmuje, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
                {
                    handle_error("msgsnd rekreacyjny ratownik przyjmuje");
                }

                printf("%s[%s] Ratownik basenu rekreacyjnego nie przyjął klienta %d%s\n", COLOR5, timestamp(), odebrany.PID, RESET);
            }
        }
        
    }
}

void* wypuszczanie()
{
    struct komunikat odebrany, wyslany;

    while(1)
    {
        print_klienci_w_basenie();

        if(msgrcv(ID_kolejki_ratownik_wypuszcza, &odebrany, sizeof(odebrany) - sizeof(long), RATOWNIK_REKREACYJNY, 0) == -1)
        {
            handle_error("msgrcv ratownik rekreacyjny wpuszcza");
        }

        if(odebrany.wiek_opiekuna==0)
        {        
            pthread_mutex_lock(&klient_mutex);

            int indeks_klienta_do_usuniecia;
            for (int i = 0; i < licznik_klientow; i++)
            {
                if (klienci_w_basenie[i] == odebrany.PID)
                {
                    indeks_klienta_do_usuniecia = i;
                    break;
                }
            }

            for (int i = indeks_klienta_do_usuniecia; i < licznik_klientow - 1; i++)
            {
                klienci_w_basenie[i] = klienci_w_basenie[i + 1];
            }
            licznik_klientow--;
            licznik_klientow_wiek--;
            
            pthread_mutex_unlock(&klient_mutex);

            suma_wieku -= odebrany.wiek;

            printf("%s[%s] Ratownik basenu rekreacyjnego wypuścił klienta %d%s\n", COLOR5, timestamp(), odebrany.PID, RESET);
        }
        else
        {        
            pthread_mutex_lock(&klient_mutex);

            int indeks_klienta_do_usuniecia;
            for (int i = 0; i < licznik_klientow; i++)
            {
                if (klienci_w_basenie[i] == odebrany.PID)
                {
                    indeks_klienta_do_usuniecia = i;
                    break;
                }
            }

            for (int i = indeks_klienta_do_usuniecia; i < licznik_klientow - 1; i++)
            {
                klienci_w_basenie[i] = klienci_w_basenie[i + 1];
            }
            licznik_klientow--;
            licznik_klientow_wiek = licznik_klientow_wiek-2;
            
            pthread_mutex_unlock(&klient_mutex);

            suma_wieku = suma_wieku - odebrany.wiek - odebrany.wiek_opiekuna;

            printf("%s[%s] Ratownik basenu rekreacyjnego wypuścił klienta %d%s\n", COLOR5, timestamp(), odebrany.PID, RESET);
        }

        wyslany.mtype = odebrany.PID;
        wyslany.PID = odebrany.PID;
        wyslany.ID_semafora = ID_semafora_rekreacyjny;

        if(msgsnd(ID_kolejki_ratownik_wypuszcza, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
        {
            handle_error("msgsnd ID_kolejki_ratownik_wypuszcza");
        }
    }
}
