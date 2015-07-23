PROGRAM = FE
LOADLIBES = 
CXX.SRCS = FE.c
CC = gcc
DEFINES = 
CXX = gcc
CPPFLAGS = -Wall $(DEFINES)
OBJS = $(CXX.SRCS:.c=.o)
all : $(PROGRAM) clean
$(PROGRAM) : $(OBJS)
	$(CC) $(CPPFLAGS) $(OBJS) $(LOADLIBES) -o $(PROGRAM)
clean:
	/bin/rm -f *.o *~
depend:
	$(CXX) -MM $(CXX.SRCS)

### <DEPENDENCIES ON .h FILES GO HERE> 
# name1.o : header1.h header2.h ...
