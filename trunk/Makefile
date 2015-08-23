CC = gcc
NAME = zwave
SRC = main.c ZWApi.c ZWCommands.c logging.c serial.c
OBJ = $(subst .c,.o, $(SRC))
LDLIBS += -pthread
LDFLAGS += -lm

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

clean:
	-rm -f $(NAME) *.d *.o
