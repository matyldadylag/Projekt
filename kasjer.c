#include "utils.c"

void SIGINT_handler(int sig);

int main()
{
    // Komunikat o uruchomieniu kasjera
    printf("%s[%s] Kasjer uruchomiony%s\n", COLOR2, timestamp(), RESET);

    // Obsługa sygnału SIGINT
    if (signal(SIGINT, SIGINT_handler) == SIG_ERR)
    {
        handle_error("kasjer: signal SIGINT_handler");
    }

    // Uzyskanie dostępu do kolejki komunikatów dla kasjera
    key_t klucz_kolejki_kasjer = ftok(".", 6377);
    if (klucz_kolejki_kasjer == -1)
    {
        handle_error("kasjer: ftok klucz_kolejki_kasjer");
    }
    int ID_kolejki_kasjer = msgget(klucz_kolejki_kasjer, IPC_CREAT | 0600);
    if (ID_kolejki_kasjer == -1)
    {
        handle_error("kasjer: msgget ID_kolejki_kasjer");
    }

    // Uzyskanie dostępu do segmentu pamięci dzielonej, która przechowuje zmienną bool czas_przekroczony
    key_t klucz_pamieci_czas_przekroczony = ftok(".", 3213);
    if(klucz_pamieci_czas_przekroczony==-1)
    {
        handle_error("kasjer: ftok klucz_pamieci_czas_przekroczony");
    }
    int ID_pamieci_czas_przekroczony = shmget(klucz_pamieci_czas_przekroczony, sizeof(bool), 0600 | IPC_CREAT);
    if(ID_pamieci_czas_przekroczony==-1)
    {
        handle_error("kasjer: shmget ID_pamieci_czas_przekroczony");
    }
    bool *czas_przekroczony = (bool*)shmat(ID_pamieci_czas_przekroczony, NULL, 0);
    if (czas_przekroczony == (void*)-1)
    {
        handle_error("kasjer: shmat czas_przekroczony");
    }

    // Uzyskanie dostępu do segmentu pamięci dzielonej, która przechowuje zmienną bool okresowe_zamkniecie
    key_t klucz_pamieci_okresowe_zamkniecie = ftok(".", 9929);
    if(klucz_pamieci_okresowe_zamkniecie==-1)
    {
        handle_error("kasjer: ftok klucz_pamieci_okresowe_zamkniecie");
    }
    int ID_pamieci_okresowe_zamkniecie = shmget(klucz_pamieci_okresowe_zamkniecie, sizeof(bool), 0600 | IPC_CREAT);
    if(ID_pamieci_okresowe_zamkniecie==-1)
    {
        handle_error("kasjer: shmget ID_pamieci_okresowe_zamkniecie");
    }
    bool *okresowe_zamkniecie = (bool*)shmat(ID_pamieci_okresowe_zamkniecie, NULL, 0);
    if (okresowe_zamkniecie == (void*)-1)
    {
        handle_error("kasjer: shmat okresowe_zamkniecie");
    }

    struct komunikat odebrany, wyslany;

    while (1)
    {
        // Odebranie wiadomości od klienta
        if (msgrcv(ID_kolejki_kasjer, &odebrany, sizeof(odebrany) - sizeof(long), -2, 0) == -1)
        {
            handle_error("kasjer: msgrcv ID_kolejki_kasjer");
        }

        // Decyzja o przyjęciu klienta
        if (*czas_przekroczony == false && *okresowe_zamkniecie == false) // Jeśli basen nie jest jeszcze zamknięty i nie jest w trakcie okresowego zamknięcia
        {
            wyslany.pozwolenie = true;
        }
        else
        {
            wyslany.pozwolenie = false;
        }
       
        // Wysłanie wiadomości
        wyslany.mtype = odebrany.PID; // Odesłanie nan PID klienta
        if (msgsnd(ID_kolejki_kasjer, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
        {
            handle_error("kasjer: msgsnd ID_kolejki_kasjer");
        }

        // Komunikat o przyjęciu lub nie klienta
        if(wyslany.pozwolenie == true)
        {
            if (odebrany.mtype == 1)
            {
                if (odebrany.wiek_opiekuna == 0)
                {
                    printf("%s[%s] Kasjer przyjął klienta VIP %d%s\n", COLOR2, timestamp(), odebrany.PID, RESET);
                }
                else
                {
                    printf("%s[%s] Kasjer przyjął opiekuna klienta VIP %d z dzieckiem%s\n", COLOR2, timestamp(), odebrany.PID, RESET);
                }
            }
            else
            {
                if (odebrany.wiek_opiekuna == 0)
                {
                    printf("%s[%s] Kasjer przyjął płatność klienta %d%s\n", COLOR2, timestamp(), odebrany.PID, RESET);
                }
                else
                {
                    printf("%s[%s] Kasjer przyjął płatność opiekuna klienta %d. Dziecko wchodzi za darmo%s\n", COLOR2, timestamp(), odebrany.PID, RESET);
                }
            }
        }
        else
        {
            printf("%s[%s] Kasjer nie przyjął klienta %d%s\n", COLOR2, timestamp(), odebrany.PID, RESET);
        }
    }
    return 0;
}

// Obsługa SIGINT
void SIGINT_handler(int sig)
{
    // Komunikat o zakończeniu działania kasjera
    printf("%s[%s] Kasjer kończy działanie%s\n", COLOR2, timestamp(), RESET);

    exit(0);
}