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
#include <errno.h>

// Długość biletu czasowego - maksymalny czas, jaki proces może spędzić na basenie (w sekundach)
#define BILET 10
// Czas przerwy jest wyliczany procentowo na podstawie tego, jak długo basen jest otwarty
#define CZAS_PRZERWY_PROCENT 0.25

// Semafory basenów - maksymalna liczba osób w danym basenie
#define MAKS_OLIMPIJSKI 30
#define MAKS_REKREACYJNY 20
#define MAKS_BRODZIK 10

// Kolejki komunikatów - adresy dla kolejek komunikatów
#define KASJER_VIP 1 // Musi mieć mniejszą wartość niż KASJER
#define KASJER 2
#define RATOWNIK_BRODZIK 11
#define RATOWNIK_REKREACYJNY 12
#define RATOWNIK_OLIMPIJSKI 13

// Struktura przechowująca dane klienta
struct dane_klienta
{
    pid_t PID;
    int wiek;
    int wiek_opiekuna;
    bool VIP;
    bool czepek;
    int wybor_basenu;
};

// Struktura komunikatu
struct komunikat
{
    long mtype;
    pid_t PID;
    int wiek;
    int wiek_opiekuna;
    bool pozwolenie;
};

void handle_error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

// Funkcje operacji semaforowych
static void semafor_v(int semafor_id, int numer_semafora)
{
    struct sembuf bufor_sem;
    bufor_sem.sem_num = numer_semafora;
    bufor_sem.sem_op = 1;
    bufor_sem.sem_flg = 0;

    if(semop(semafor_id, &bufor_sem, 1)==-1)
    {
        handle_error("semop V");
    }
}

static void semafor_p(int semafor_id, int numer_semafora)
{
    struct sembuf bufor_sem;
    bufor_sem.sem_num = numer_semafora;
    bufor_sem.sem_op = -1;
    bufor_sem.sem_flg = 0;
    
    if(semop(semafor_id, &bufor_sem, 1)==-1)
    {
        handle_error("semop P");
    }
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
const char *COLOR1 = "\033[38;2;228;3;3m";
const char *COLOR2 = "\033[38;2;255;140;0m";
const char *COLOR3 = "\033[38;2;255;237;0m";
const char *COLOR4 = "\033[38;2;0;138;0m";
const char *COLOR5 = "\033[38;2;0;72;255m";
const char *COLOR6 = "\033[38;2;117;5;135m";