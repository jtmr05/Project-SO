# ----------//----------
# This is a C makefile.|
# ----------//----------



#directories
SRC_DIR 			:= src
INC_DIR 			:= include
OBJ_DIR 			:= obj
BIN_DIR 			:= bin
TMP_DIR 			:= tmp



#files
SERVER_SRC_FILES	:= $(shell find $(SRC_DIR)/server -name *.c)
SERVER_OBJ_FILES	:= $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SERVER_SRC_FILES))
SERVER_BIN		 	:= $(BIN_DIR)/sdstored

CLIENT_SRC_FILES	:= $(shell find $(SRC_DIR)/client -name *.c)
CLIENT_OBJ_FILES	:= $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(CLIENT_SRC_FILES))
CLIENT_BIN		 	:= $(BIN_DIR)/sdstore

EXECS_SRC_FILES		:= $(shell find $(SRC_DIR)/execs -name *.c)
EXECS_OBJ_FILES		:= $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(EXECS_SRC_FILES))
EXECS_BIN_FILES		:= $(patsubst $(OBJ_DIR)/%.o,$(BIN_DIR)/%,$(EXECS_OBJ_FILES))


#compiler
CC 					:= gcc

#compiler flags
CFLAGS 				:= -I$(INC_DIR) -Wall -Wextra -Wsign-conversion -g

#linker flags (e.g. -L/path/to/lib)
LDFLAGS				:=

#linker libraries (e.g. -lm)
LDLIBS				:=



#make default goal (using make with no specified rule)
.DEFAULT_GOAL 		:= all

all: server client execs
	@mkdir -p $(TMP_DIR)

build: clean all



server: $(SERVER_BIN)

$(SERVER_BIN): $(SERVER_OBJ_FILES)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)



client: $(CLIENT_BIN)

$(CLIENT_BIN): $(CLIENT_OBJ_FILES)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)



execs: $(EXECS_BIN_FILES)

$(EXECS_BIN_FILES): $(BIN_DIR)/% : $(OBJ_DIR)/%.o
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)



#generate each object file according to the corresponding source file
#create directories as needed
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@





#'clean' doesn't represent an actual file generating rule
.PHONY: clean

clean:
	-rm -rf $(OBJ_DIR)
	-rm -rf $(BIN_DIR)
	-rm -rf $(TMP_DIR)