all: zarzadca kasjer klient ratownik_basen_olimpijski ratownik_brodzik ratownik_basen_rekreacyjny

zarzadca: zarzadca.o
	gcc zarzadca.o -o zarzadca

zarzadca.o: zarzadca.c header.h
	gcc zarzadca.c -c

kasjer: kasjer.o
	gcc kasjer.o -o kasjer

kasjer.o: kasjer.c header.h
	gcc kasjer.c -c

klient: klient.o
	gcc klient.o -o klient

klient.o: klient.c header.h
	gcc klient.c -c

ratownik_basen_olimpijski: ratownik_basen_olimpijski.o
	gcc ratownik_basen_olimpijski.o -o ratownik_basen_olimpijski -lpthread

ratownik_basen_olimpijski.o: ratownik_basen_olimpijski.c header.h
	gcc ratownik_basen_olimpijski.c -c

ratownik_brodzik: ratownik_brodzik.o
	gcc ratownik_brodzik.o -o ratownik_brodzik -lpthread

ratownik_brodzik.o: ratownik_brodzik.c header.h
	gcc ratownik_brodzik.c -c

ratownik_basen_rekreacyjny: ratownik_basen_rekreacyjny.o
	gcc ratownik_basen_rekreacyjny.o -o ratownik_basen_rekreacyjny -lpthread

ratownik_basen_rekreacyjny.o: ratownik_basen_rekreacyjny.c header.h
	gcc ratownik_basen_rekreacyjny.c -c

clean:
	rm -f *.o