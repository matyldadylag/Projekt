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

#define BILET 5 // Maksymalny czas, jaki proces może spędzić na basenie (w sekundach)

// Semafory basenów
#define MAKS_OLIMPIJSKI 2 // Maksymalna liczba osób w basenie olimpijskim
#define MAKS_REKREACYJNY 2 // Maksymalna liczba osób w basenie rekreacyjnym
#define MAKS_BRODZIK 2 // Maksymalna liczba osób w brodziku

// Kolejki komunikatów
#define MAKS_DLUG_KOM 255 // Maksymalna długość przekazywanych komunikatów
// Makrosy adresatów kolejek komunikatów
#define KASJER 1
#define RATOWNIK_OLIMPIJSKI_PRZYJMUJE 211
#define RATOWNIK_OLIMPIJSKI_WYPUSZCZA 212
#define RATOWNIK_REKREACYJNY_PRZYJMUJE 221
#define RATOWNIK_REKREACYJNY_WYPUSZCZA 222
#define RATOWNIK_BRODZIK_PRZYJMUJE 231
#define RATOWNIK_BRODZIK_WYPUSZCZA 232

// Struktura komunikatu
struct komunikat
{
    long mtype;
    pid_t ktype;
    int wiek;
    int wiek_opiekuna;
    int VIP;
    bool moze_wejsc;
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