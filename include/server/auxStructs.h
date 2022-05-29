#ifndef AUXSTRUCTS_H
#define AUXSTRUCTS_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "signal.h"
#include "sys/wait.h"

#define MESSAGE_SIZE 4096

/** Command **/

typedef struct command{
    char *type; //Tipo do comando (nop,gcompress,etc)
    int max;
    int running;
} *Command;

Command newCommand(char *line);

void incRunningCommand(Command c);

void decRunningCommand(Command c);

int commandAvailable (Command c);

void printCommand(int fildes, Command c);

// Linked List of Commands

typedef struct llCommand{
    Command command;
    struct llCommand *next;
} *LlCommand;

LlCommand newLLC(Command c);

void freeLLC(LlCommand list);

void appendLLC(LlCommand list, Command c);

Command getCommand(LlCommand, char *type);

/** Lista ligada com a informacao dos processos **/

typedef struct linkedListProcess{
    pid_t pid_client;
    pid_t pid_child; //Pid do filho que vai executar a task
    int task_number;
    char *input_file;
    char *output_file;
    int commandsCount;
    int priority;
    char **commands;
    struct linkedListProcess *next;
} *LinkedListProcess;

//LinkedListProcess newProcess(pid_t pid, int task_number, char *input_file, char *output_file, int commandsCount, char **commands);

LinkedListProcess parseProcess(char *str, int task_number);

void freeProcess(LinkedListProcess process);

void appendsProcess(LinkedListProcess l, LinkedListProcess p);

LinkedListProcess removeProcessesHead(LinkedListProcess *list);

//Dá print da info do processo, para o ficheiro fornecido
//Admite 'process' não nulo
void printProcessInfo(int fildes, LinkedListProcess process);

LinkedListProcess removeProcessByChildPid(LinkedListProcess *list, pid_t pid);

/** Outras **/

ssize_t readln(int fd, char* line, size_t size);

LlCommand read_commands_config_file(char *filepath);

//Retorna:
// -1 se o processo superar os limites do servidor.  Por exemplo, se for pedido para aplicar o command 'nop' 4x, e o servidor tiver um máximo de 3.
//  0 se o processo não puder ser corrido de momento
//  1 se o processo puder ser corrido neste momento
int isTaskRunnable (LlCommand llc, LinkedListProcess process);

#endif