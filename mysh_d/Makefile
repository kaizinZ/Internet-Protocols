CC = gcc

TARGET = mysh
OBJS = main.o

RM = rm -f

$(TARGET): $(OBJS)
		$(CC) -o $@ $(OBJS)

.c.o:
		gcc -c $<

clean:
		$(RM) $(OBJS)

clean_target:
		$(RM) $(TARGET)

clean_all:
		$(RM) $(TARGET) $(OBJS)