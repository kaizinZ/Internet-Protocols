CC = gcc

all: OBJS1 OBJS2

OBJS1: myftpc.c
	gcc -o myftpc myftpc.c

OBJS2: myftpd.c
	gcc -o myftpd myftpd.c

RM = rm -f

clean:
		$(RM) myftpd myftpc 

clean_target:
		$(RM) $(TARGET)

clean_all:
		$(RM) $(TARGET) $(OBJS1) $(OBJS2) 