#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>

#define MAX 255
#define SERWER 1

void handle_error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int ID_kolejki;

void handle_sigint(int sig)
{
    if (msgctl(ID_kolejki, IPC_RMID, 0) == -1)
    {
		handle_error("msgctl");
	}
    exit(0);
}

struct komunikat
{
    long mtype;
    pid_t ktype;
    char mtext[MAX];
};

int main()
{
    if (signal(SIGINT, handle_sigint) == SIG_ERR) 
    {
        handle_error("signal");
    }
    
    key_t key = ftok(".", 17);
	
    ID_kolejki = msgget(key, IPC_CREAT | 0600);
    if (ID_kolejki == -1)
    {
        handle_error("msgget");
    }

    struct komunikat odebrany, wyslany;
    int i;

    while (1)
    {
        printf("Serwer czeka na komunikat...\n");

        odebrany.mtype = SERWER;
        if (msgrcv(ID_kolejki, &odebrany, sizeof(odebrany) - sizeof(long), odebrany.mtype, 0) == -1)
        {
			handle_error("msgrcv");
        }

        printf("Serwer odebral komunikat %s od %d\n", odebrany.mtext, odebrany.ktype);

        wyslany.mtype = odebrany.ktype;
        wyslany.ktype = odebrany.ktype;

        for (i = 0; i < strlen(odebrany.mtext); i++)
        {
            wyslany.mtext[i] = toupper(odebrany.mtext[i]);
        }

        printf("Serwer wysyla komunikat %s do %d\n", wyslany.mtext, wyslany.ktype);
        if (msgsnd(ID_kolejki, &wyslany, sizeof(struct komunikat) - sizeof(long), 0) == -1)
        {
			handle_error("msgsnd");
        }
    }

    return 0;
}