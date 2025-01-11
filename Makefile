all: zarzadca klient kasjer ratownik
zarzadca: zarzadca.o
	gcc -o zarzadca zarzadca.o
zarzadca.o: zarzadca.c
	gcc -c zarzadca.c
klient: klient.o
	gcc -o klient klient.o
klient.o: klient.c
	gcc -c klient.c
kasjer: kasjer.o
	gcc -o kasjer kasjer.o
kasjer.o: kasjer.c
	gcc -c kasjer.c
ratownik: ratownik.o
	gcc -o ratownik ratownik.o -lpthread
ratownik.o: ratownik.c
	gcc -c ratownik.c
clean:
	rm -f *.o