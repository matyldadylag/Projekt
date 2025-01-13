all: zarzadca klient kasjer ratownik_basen_olimpijski ratownik_brodzik ratownik_basen_rekreacyjny

zarzadca: zarzadca.o
	gcc -o zarzadca zarzadca.o

zarzadca.o: zarzadca.c header.h
	gcc -c zarzadca.c

klient: klient.o
	gcc -o klient klient.o

klient.o: klient.c header.h
	gcc -c klient.c

kasjer: kasjer.o
	gcc -o kasjer kasjer.o

kasjer.o: kasjer.c header.h
	gcc -c kasjer.c

ratownik_basen_olimpijski: ratownik_basen_olimpijski.o
	gcc -o ratownik_basen_olimpijski ratownik_basen_olimpijski.o -lpthread

ratownik_basen_olimpijski.o: ratownik_basen_olimpijski.c header.h
	gcc -c ratownik_basen_olimpijski.c

ratownik_brodzik: ratownik_brodzik.o
	gcc -o ratownik_brodzik ratownik_brodzik.o -lpthread

ratownik_brodzik.o: ratownik_brodzik.c header.h
	gcc -c ratownik_brodzik.c

ratownik_basen_rekreacyjny: ratownik_basen_rekreacyjny.o
	gcc -o ratownik_basen_rekreacyjny ratownik_basen_rekreacyjny.o -lpthread

ratownik_basen_rekreacyjny.o: ratownik_basen_rekreacyjny.c header.h
	gcc -c ratownik_basen_rekreacyjny.c

clean:
	rm -f *.o