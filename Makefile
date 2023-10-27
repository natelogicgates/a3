# CXX Make variable for compiler
CC=g++
# -std=c++11  C/C++ variant to use, e.g. C++ 2011
# -Wall       show the necessary warning files
# -g3         include information for symbolic debugger e.g. gdb 
CCFLAGS=-std=c++11 -Wall -g3 -c

# object files
OBJS = log_helpers.o demandpaging.o 

# Program name
PROGRAM = demandpaging

# The program depends upon its object files
$(PROGRAM) : $(OBJS)
	$(CC) -o $(PROGRAM) $(OBJS)

demandpaging.o : demandpaging.cpp demandpaging.h
	$(CC) $(CCFLAGS) demandpaging.cpp
	
log_helpers.o: log_helpers.c log_helpers.h
	$(CC) $(CCFLAGS) log_helpers.c
# Once things work, people frequently delete their object files.
# If you use "make clean", this will do it for you.
# As we use gnuemacs which leaves auto save files termintating
# with ~, we will delete those as well.
clean :
	rm -f *.o *~ $(PROGRAM)
