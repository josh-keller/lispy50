# -*- MakeFile -*-

lispy: lispy.o mpc.o lval.o lenv.o builtins.o
	cc -std=c99 -Wall lispy.o mpc.o lval.o lenv.o builtins.o -ledit -lm -o lispy

lispy.o: lispy.c mpc.h lval.h lenv.h builtins.h
	cc -std=c99 -c -Wall lispy.c

builtins.o: builtins.c builtins.h lval.o lenv.o 
	cc -std=c99 -c -Wall builtins.c

lenv.o: lenv.c lenv.h lval.o
	cc -std=c99 -c -Wall lenv.c

lval.o: lval.c lval.h
	cc -std=c99 -c -Wall lval.c

mpc.o: mpc.c mpc.h
	cc -std=c99 -c -Wall mpc.c

clean:
	rm *.o lispy
