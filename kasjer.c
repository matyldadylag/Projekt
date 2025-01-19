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

    // Utworzenie kolejki komunikatów dla kasjera
    key_t klucz_kolejki_kasjer = ftok(".", 6377);
    ID_kolejki_kasjer = msgget(klucz_kolejki_kasjer, IPC_CREAT | 0600);

    struct komunikat odebrany, wyslany;

    while(1)
    {
        // Odebranie wiadomości od klienta
        // Kasjer ma w pierwszeństwie obsłużyć VIP (mtype 1). Zgodnie z działaniem kolejki obsługuje w pierwszeństwie najmnniejsze liczby <|-2|
        msgrcv(ID_kolejki_kasjer, &odebrany, sizeof(odebrany) - sizeof(long), -2, 0);
    
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

        // Kasjer zmienia wartości, aby wiadomość dotarła na PID klienta
        wyslany.PID = odebrany.PID;

        // Wysłanie wiadomości
        msgsnd(ID_kolejki_kasjer, &wyslany, sizeof(struct komunikat) - sizeof(long), 0);
    }

    return 0;
}