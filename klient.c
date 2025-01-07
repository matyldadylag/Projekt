#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <unistd.h>

#define MAX 255
#define SERWER 1
#define MSGMAX 8192

int ID_kolejki;

void* wysylanie();
void* odbieranie();

struct komunikat
{
    long mtype;
    pid_t ktype;
    char mtext[MAX];
};

void handle_error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int main()
{
    key_t key = ftok(".", 17);

    ID_kolejki = msgget(key, IPC_CREAT | 0600);
    if (ID_kolejki == -1)
    {
		handle_error("msgget");
    }

    pthread_t wysyla, odbiera;
    if (pthread_create(&wysyla, NULL, wysylanie, NULL) != 0 || pthread_create(&odbiera, NULL, odbieranie, NULL) != 0)
    {
		handle_error("pthread_create");
    }

    if (pthread_join(wysyla, NULL) != 0 || pthread_join(odbiera, NULL))
    {
        handle_error("pthread_join");
    }

	return 0;
}


void* wysylanie()
{
    struct komunikat kom;
    kom.mtype = SERWER;
    kom.ktype = getpid();

    sprintf(kom.mtext, "Jestem klientem %d", getpid());
	
	printf("Klient %d wysyla komunikat\n", kom.ktype);
	if (msgsnd(ID_kolejki, &kom, sizeof(kom) - sizeof(long), 0) == -1)
	{
		handle_error("msgsnd");	
	}
    
    pthread_exit(NULL);
}

void* odbieranie()
{
    struct komunikat kom;
    kom.ktype = getpid();

	if (msgrcv(ID_kolejki, &kom, sizeof(struct komunikat) - sizeof(long), kom.ktype, 0) == -1)
	{
		handle_error("msgrcv");
	}
	
	printf("Klient %d odebral komunikat: %s\n", kom.ktype, kom.mtext);
    
    pthread_exit(NULL);
}