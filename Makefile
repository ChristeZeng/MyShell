CC = g++ -std=c++11

myshell: MyShell.cpp MyShell.h Buildin.h
	$(CC) -o myshell MyShell.cpp MyShell.h Buildin.h