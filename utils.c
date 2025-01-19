#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>
#include <pthread.h>

// Długość biletu czasowego - maksymalny czas, jaki proces może spędzić na basenie (w sekundach)
#define BILET 5

// Semafory basenów - maksymalna liczba osób w danym basenie
#define MAKS_OLIMPIJSKI 2
#define MAKS_REKREACYJNY 2
#define MAKS_BRODZIK 2

// Kolejki komunikatów
#define MAKS_DLUG_KOM 255 // Maksymalna długość przekazywanych komunikatów
// Makrosy adresatów kolejek komunikatów
#define KASJER_VIP 1 // Musi mieć mniejszą wartość niż KASJER
#define KASJER 2
#define RATOWNIK_OLIMPIJSKI 11
#define RATOWNIK_REKREACYJNY 12
#define RATOWNIK_BRODZIK 13

// Struktura przechowująca dane klienta
struct dane_klienta
{
    pid_t PID;
    int wiek;
    int wiek_opiekuna;
    bool VIP;
    bool czepek;
    int wybor_basenu;
    int drugi_wybor_basenu;
};

// Struktura komunikatu
struct komunikat
{
    long mtype;
    pid_t PID;
    int wiek;
    int wiek_opiekuna;
    bool pozwolenie;
    char mtext[MAKS_DLUG_KOM];
};

// Funkcje operacji semaforowych dla zwykłego klienta
static void semafor_v(int semafor_id, int numer_semafora)
{
    struct sembuf bufor_sem;
    bufor_sem.sem_num = numer_semafora;
    bufor_sem.sem_op = 1;
    bufor_sem.sem_flg = SEM_UNDO;

    semop(semafor_id, &bufor_sem, 1);
}

static void semafor_p(int semafor_id, int numer_semafora)
{
    struct sembuf bufor_sem;
    bufor_sem.sem_num = numer_semafora;
    bufor_sem.sem_op = -1;
    bufor_sem.sem_flg = 0;
    
    semop(semafor_id, &bufor_sem, 1);
}

// Funkcje operacji semaforowych dla klienta z dzieckiem - zwiększają/obniżają wartość semaforów o 2
static void semafor_v_2(int semafor_id, int numer_semafora)
{
    struct sembuf bufor_sem;
    bufor_sem.sem_num = numer_semafora;
    bufor_sem.sem_op = 2;
    bufor_sem.sem_flg = SEM_UNDO;

    semop(semafor_id, &bufor_sem, 1);
}

static void semafor_p_2(int semafor_id, int numer_semafora)
{
    struct sembuf bufor_sem;
    bufor_sem.sem_num = numer_semafora;
    bufor_sem.sem_op = -2;
    bufor_sem.sem_flg = 0;
    
    semop(semafor_id, &bufor_sem, 1);
}

// Pomocnicza funkcja do wyświetlania timestampów w wiadomościach
char* timestamp()
{
	static char time_str[9];
	strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&(time_t){time(NULL)}));
	return time_str;
}

// Kolorowanie tekstu
const char *RESET = "\033[0m";
const char *RED = "\033[31m";
const char *GREEN = "\033[32m";
const char *YELLOW = "\033[33m";
const char *BLUE = "\033[34m";
const char *MAGENTA = "\033[35m";