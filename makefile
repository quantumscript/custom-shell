# Makefile
CXX = gcc
CXXFLAGS += -g

TARGET = smallsh

SRCS = swapShellPID.c tstpHandler.c smallsh.c

HEADERS = functions.h

OBJS = ${SRCS:.c=.o}

${TARGET}: ${OBJS} ${HEADERS}
	${CXX} ${OBJS} -o ${TARGET}
	make clean

${OBJS}: ${SRCS}
	${CXX} ${CXXFLAGS} -c ${SRCS:.o=.c}

clean:
	rm -f *.o
