all: zarzadca kasjer klient ratownik_basen_olimpijski ratownik_brodzik ratownik_basen_rekreacyjny

zarzadca: zarzadca.o
	gcc zarzadca.o -o zarzadca -lpthread

zarzadca.o: zarzadca.c utils.c
	gcc zarzadca.c -c

kasjer: kasjer.o
	gcc kasjer.o -o kasjer

kasjer.o: kasjer.c utils.c
	gcc kasjer.c -c

klient: klient.o
	gcc klient.o -o klient -lpthread

klient.o: klient.c utils.c
	gcc klient.c -c

ratownik_basen_olimpijski: ratownik_basen_olimpijski.o
	gcc ratownik_basen_olimpijski.o -o ratownik_basen_olimpijski -lpthread

ratownik_basen_olimpijski.o: ratownik_basen_olimpijski.c utils.c
	gcc ratownik_basen_olimpijski.c -c

ratownik_brodzik: ratownik_brodzik.o
	gcc ratownik_brodzik.o -o ratownik_brodzik -lpthread

ratownik_brodzik.o: ratownik_brodzik.c utils.c
	gcc ratownik_brodzik.c -c

ratownik_basen_rekreacyjny: ratownik_basen_rekreacyjny.o
	gcc ratownik_basen_rekreacyjny.o -o ratownik_basen_rekreacyjny -lpthread

ratownik_basen_rekreacyjny.o: ratownik_basen_rekreacyjny.c utils.c
	gcc ratownik_basen_rekreacyjny.c -c

clean:
	rm -f *.o