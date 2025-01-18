#include "utils.c"

// Zdefiniowane wyżej, aby być w stanie usunąć struktury asynchronicznie w przypadku SIGINT
pid_t ID_kolejki_kasjer;

// Obsługa SIGINT
void SIGINT_handler(int sig)
{
    // Usunięcie kolejki komunikatów
    msgctl(ID_kolejki_kasjer, IPC_RMID, 0);

    exit(0);
}

int main()
{
    signal(SIGINT, SIGINT_handler);

    // Uzyskanie dostępu do pamięci dzielonej do przechowywania zmiennej czas_otwarcia
    key_t klucz_pamieci = ftok(".", 3213);
    int ID_pamieci = shmget(klucz_pamieci, sizeof(time_t), 0600 | IPC_CREAT);
    time_t* czas_otwarcia = (time_t*)shmat(ID_pamieci, NULL, 0);
    
    // Utworzenie kolejki dla kasjera
    key_t klucz_kolejki_kasjer = ftok(".", 6377);
    ID_kolejki_kasjer = msgget(klucz_kolejki_kasjer, IPC_CREAT | 0600);

    struct komunikat odebrany, wyslany;

    int wiek, wiek_opiekuna;

    while(1)
    {
        // Odebranie wiadomości od klienta
        // mtype jest ustawiony na -2: to oznacza, że kasjer najpierw odbierze komunikaty o mtype 1, a potem komunikaty o mtype 2
        msgrcv(ID_kolejki_kasjer, &odebrany, sizeof(odebrany) - sizeof(long), -2, 0);
    
        if (odebrany.mtype == 1)
        {
            printf("[%s] Kasjer: obsługuję klienta VIP %d\n", timestamp(), odebrany.ktype);
        }
        
        if (odebrany.mtype == 2)
        {
            printf("[%s] Kasjer: obsługuję klienta %d\n", timestamp(), odebrany.ktype);
        }
        
        if(odebrany.wiek_opiekuna == 0)
        {
            printf("[%s] Kasjer: przyjmuję płatność %d\n", timestamp(), odebrany.ktype);
        }
        else
        {
            printf("[%s] Kasjer: przyjmuję płatność opiekuna %d. Dziecko wchodzi za darmo\n", timestamp(), odebrany.ktype);
        }


        // Kasjer zmienia wartości, aby wiadomość dotarła na PID klienta
        wyslany.mtype = odebrany.ktype;
        wyslany.ktype = odebrany.ktype;
        wyslany.moze_wejsc = true;

        // Wysłanie wiadomości
        msgsnd(ID_kolejki_kasjer, &wyslany, sizeof(struct komunikat) - sizeof(long), 0);
    }

    return 0;
}