EXEC_NAME = freshsync
OBJ_FILES = freshsync.o
 
INCLUDES = -I.
LIBS = -lssh2
LIBS_DIR = -L.
CC = gcc
CFLAGS =
 
all : $(EXEC_NAME)
 
clean :
	@rm $(EXEC_NAME) $(OBJ_FILES)
	@echo "Cleaned"

install : $(EXEC_NAME)
	@cp $(EXEC_NAME) /usr/local/bin/

$(EXEC_NAME) : $(OBJ_FILES)
	@$(CC) -O3 -o $(EXEC_NAME) $(OBJ_FILES) $(LIBS_DIR) $(INCLUDES) $(LIBS)
	@echo "Compiled"
