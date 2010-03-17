CFLAGS = -g -Wall  -static

debit: debit.o
	gcc $(CFLAGS) -o debit debit.o

debit.o: debit.c
	gcc $(CFLAGS) -c -o debit.o debit.c
