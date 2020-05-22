# -*- MakeFile -*-

lispy: lispy.o mpc.o
	cc -std=c99 -Wall lispy.o mpc.o -ledit -lm -o lispy

lispy.o: lispy.c mpc.h
	cc -std=c99 -c -Wall lispy.c

mpc.o: mpc.c mpc.h
	cc -std=c99 -c -Wall mpc.c


