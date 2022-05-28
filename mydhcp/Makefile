CC = gcc

all: OBJS1 OBJS2

OBJS1: dhcps.c
	gcc -o mydhcps dhcps.c

OBJS2: dhcpc.c
	gcc -o mydhcpc dhcpc.c

RM = rm -f

clean:
		$(RM) mydhcpc mydhcps 

clean_target:
		$(RM) $(TARGET)

clean_all:
		$(RM) $(TARGET) $(OBJS1) $(OBJS2) 