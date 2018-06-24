###########################################################
#######                 color                       #######
BLACK 		= "\e[30;1m"
RED  		= "\e[31;1m"
GREEN 		= "\e[32;1m"
YELLOW 		= "\e[33;1m"
BLUE  		= "\e[34;1m"
PURPLE 		= "\e[35;1m"
CYAN  		= "\e[36;1m"
WHITE 		= "\e[37;1m"
COLOR_END	= "\e[0m"
###########################################################
.SUFFIXES: .o .c
.c.o:
	$(CC) $(CFLAGS) -c $< -Wall -Wno-pointer-sign
###########################################################
SHELL = /bin/bash
CC    = gcc
###########################################################
INCL    = -I.
CFLAGS	= -DDEBUG -g -fPIC $(INCL)
LIBS	= -pthread -lrt
GOAL	= libfile_op.so
OBJ		= file_op.o

all:$(GOAL) clear

$(GOAL) : $(OBJ)
	$(CC) -g -o $@ $? -shared $(LIBS)
	@echo -e $(BLUE)"\t\t[$@] compilied success"$(COLOR_END)

clear:
	rm -f *.o



