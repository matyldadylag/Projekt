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

// Czas (w sekundach)
#define CZAS_BILETU 15
#define CZAS_PRACY 60

// Semafory
#define MAKS_LICZBA_KLIENTOW 2

// Kolejki komunikatów
#define MAKS_DLUGOSC_KOMUNIKATU 255
// Kolejki komunikatów - Adresaci
#define KASJER 1
#define RATOWNIK_BRODZIK_PRZYJMUJE 211
#define RATOWNIK_BRODZIK_WYPUSZCZA 212
#define RATOWNIK_OLIMPIJSKI_PRZYJMUJE 221
#define RATOWNIK_OLIMPIJSKI_WYPUSZCZA 222
#define RATOWNIK_REKREACYJNY_PRZYJMUJE 231
#define RATOWNIK_REKREACYJNY_WYPUSZCZA 232

// Struktura komunikatu
struct komunikat
{
    long mtype;
    pid_t ktype;
    char mtext[MAKS_DLUGOSC_KOMUNIKATU];
};

// Funkcje semafora
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

char* timestamp()
{
	static char time_str[9];
	strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&(time_t){time(NULL)}));
	return time_str;
}