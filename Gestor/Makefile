Gestor:Gestor.o aux.o
	gcc -Wall -O3 -o Gestor Gestor.o aux.o 
aux.o:aux.c aux.h
	gcc -c -Wall aux.c
Gestor.o:Gestor.c aux.h
	gcc -c -Wall Gestor.c
clean:
	rm *.o
