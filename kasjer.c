#include "utils.c"

// Zdefiniowane wyżej, aby być w stanie usunąć struktury asynchronicznie w przypadku SIGINT
pid_t ID_kolejki_kasjer;

// Obsługa SIGINT
void SIGINT_handler(int sig)
{
    // Usunięcie kolejki komunikatów
    msgctl(ID_kolejki_kasjer, IPC_RMID, 0);

    // Komunikat o zakończeniu działania kasjera
    printf("%s[%s] Kasjer kończy działanie%s\n", COLOR3, timestamp(), RESET);

    exit(0);
}

int main()
{
    signal(SIGINT, SIGINT_handler);

    // Komunikat o uruchomieniu kasjera
    printf("%s[%s] Kasjer uruchomiony%s\n", COLOR3, timestamp(), RESET);

    // Uzyskanie dostępu do segmentu pamięci dzielonej, która przechowuje zmienną bool czas_przekroczony
    key_t klucz_pamieci = ftok(".", 3213);
    int ID_pamieci = shmget(klucz_pamieci, sizeof(bool), 0600 | IPC_CREAT);
    bool* czas_przekroczony = (bool*)shmat(ID_pamieci, NULL, 0);

    // Utworzenie kolejki komunikatów dla kasjera
    key_t klucz_kolejki_kasjer = ftok(".", 6377);
    ID_kolejki_kasjer = msgget(klucz_kolejki_kasjer, IPC_CREAT | 0600);

    struct komunikat odebrany, wyslany;

    while(1)
    {
        // Odebranie wiadomości od klienta
        // Kasjer ma w pierwszeństwie obsłużyć VIP (mtype 1). Zgodnie z działaniem kolejki obsługuje w pierwszeństwie najmnniejsze liczby <|-2|
        msgrcv(ID_kolejki_kasjer, &odebrany, sizeof(odebrany) - sizeof(long), -2, 0);

        if(*czas_przekroczony == false) // Jeśli basen nie jest jeszcze zamknięty
        {
            if (odebrany.mtype == 1)
            {
                if (odebrany.wiek_opiekuna == 0)
                {
                    printf("%s[%s] Kasjer przyjął klienta VIP %d%s\n", COLOR3, timestamp(), odebrany.PID, RESET);
                }
                else
                {
                    printf("%s[%s] Kasjer przyjął opiekuna klienta VIP %d z dzieckiem%s\n", COLOR3, timestamp(), odebrany.PID, RESET);
                }
            }
            else
            {
                if (odebrany.wiek_opiekuna == 0)
                {
                    printf("%s[%s] Kasjer przyjął płatność klienta %d%s\n", COLOR3, timestamp(), odebrany.PID, RESET);
                }
                else
                {
                    printf("%s[%s] Kasjer przyjął płatność opiekuna klienta %d. Dziecko wchodzi za darmo%s\n", COLOR3, timestamp(), odebrany.PID, RESET);
                }
            }

            // Kasjer przyjmuje klienta
            wyslany.pozwolenie = true;
        }
        else
        {
            printf("%s[%s] Kasjer nie przyjął klienta %d%s\n", COLOR3, timestamp(), odebrany.PID, RESET);

            // Kasjer nie przyjmuje klienta
            wyslany.pozwolenie = false;
        }
        
        // Kasjer zmienia wartości, aby wiadomość dotarła na PID klienta
        wyslany.mtype = odebrany.PID;
        wyslany.PID = odebrany.PID;

        // Wysłanie wiadomości
        msgsnd(ID_kolejki_kasjer, &wyslany, sizeof(struct komunikat) - sizeof(long), 0);
    }

    return 0;
}