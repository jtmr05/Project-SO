#include "server/auxStructs.h"

/** Command **/

Command newCommand(char *line){
    Command c = (Command) malloc(sizeof(struct command));
    c->type     = strdup(strsep(&line," "));
    sscanf(strsep(&line," "), "%d", &c->max);
    c->running = 0;
    return c;
}

void incRunningCommand(Command c){
    if(c == NULL) return;
    c->running++;
}

void decRunningCommand(Command c){
    if(c == NULL) return;
    c->running--;
}

int commandAvailable (Command c){
    if(c->running < c->max) return 0;
    else return -1;
}

void printCommand(int fildes, Command c){
    char buffer[256];
    sprintf(buffer,"command %s: %d/%d (running/max)\n",c->type, c->running, c->max);
    write(fildes, buffer, strlen(buffer));
}

void freeCommand(Command c){
    if(c != NULL){
        free(c->type);
        free(c);
    }
}

/** Linked List of Commands **/

LlCommand newLLC(Command c){
    LlCommand l = malloc(sizeof(struct llCommand));
    l->command = c;
    l->next   = NULL;
    return l;
}

void freeLLC(LlCommand list){
    LlCommand tmp = list;

    while(tmp != NULL){
        tmp = tmp->next;
        freeCommand(list->command);
        free(list);
        list = tmp;
    }
}

void appendsLLC(LlCommand llc, Command c){
    LlCommand l = newLLC(c);
    while (llc->next != NULL)
        llc = llc->next;
    llc->next = l;
}

Command getCommand(LlCommand llc, char *type){
    //Percorre a linked list, à procura do command
    LlCommand tmp;
    for(tmp = llc; tmp != NULL && strcmp(tmp->command->type, type) != 0; tmp = tmp->next);

    //Se tmp != NULL então encontrou o comando
    if(tmp != NULL) return tmp->command;

    return NULL;
}

/** Lista ligada com a informacao dos processos **/

LinkedListProcess parseProcess(char *str, int task_number){
    LinkedListProcess p = (LinkedListProcess) malloc(sizeof(struct linkedListProcess));
    p->pid_child    = -1;
    p->task_number  = task_number;
    p->commandsCount = 0;
    p->next         = NULL;

    //ignora "pid:"
    strsep(&str," ");

    //Gets pid from client
    p->pid_client = atoi(strsep(&str," "));

    //ignora "transform"
    strsep(&str,"\n");

    //gets input & output files
    p->input_file   = strdup(strsep(&str,"\n"));
    p->output_file  = strdup(strsep(&str,"\n"));
    p->priority  = atoi(strsep(&str,"\n"));

    //gets commands
    int maxCommands = 1;
    p->commands = (char **) malloc(maxCommands * sizeof(char *));
    char *command;

    while((command = strsep(&str,"\n")) != NULL){
        //Caso de não haver espaço suficiente para os comandos
        if(p->commandsCount == maxCommands){
            maxCommands *= 2;
            p->commands = realloc(p->commands, maxCommands * sizeof(char *));
        }

        //Coloca o comando na estrutura
        p->commands[p->commandsCount] = strdup(command);
        p->commandsCount++;
    }

    return p;
}

void freeProcess(LinkedListProcess process){
    if(process != NULL){
        free(process->input_file);
        free(process->output_file);
        for(int i = 0; i < process->commandsCount ; i++)
            free(process->commands[i]);
        free(process->commands);
        free(process);
    }
}

//Appends process to the linked list
void appendsProcess(LinkedListProcess l, LinkedListProcess p){

    LinkedListProcess next = l->next;

    while(next != NULL && l->priority >= p->priority){
        l = l->next;
        next = l->next;
    }

    p->next = next;
    l->next = p;
}

//Dá print da info do processo, para o ficheiro fornecido
//Admite 'process' não nulo
void printProcessInfo(int fildes, LinkedListProcess process){
    char buffer[1024];

    sprintf(buffer, "task #%d: process %s %s", process->task_number, process->input_file, process->output_file);

    for(int i = 0; i < process->commandsCount ; i++) {
        strcat(buffer, " ");
        strcat(buffer,process->commands[i]);
    }

    strcat(buffer,"\n");

    write(fildes, buffer, strlen(buffer));
}

LinkedListProcess removeProcessByChildPid(LinkedListProcess *list, pid_t pid){
    if(list == NULL || *list == NULL) return NULL;

    LinkedListProcess prev = NULL, tmp;

    for(tmp = *list; tmp != NULL && tmp->pid_child != pid ; prev = tmp, tmp = tmp->next);

    if(tmp != NULL){
        //Se prev for NULL, entao o pid foi encontrado no primeiro elemento da lista
        if(prev == NULL){
            *list = tmp->next;
        }
        else{
            prev->next = tmp->next;
        }

        tmp->next = NULL;
        return tmp;
    }

    return NULL;
}

LinkedListProcess removeProcessesHead(LinkedListProcess *list){
    LinkedListProcess tmp = *list;
    *list = tmp->next;
    tmp->next = NULL;
    return tmp;
}

/** Outros **/

ssize_t readln(int fd, char* line, size_t size) {
    ssize_t bytes_read = read(fd, line, size);
    if(!bytes_read) return 0;

    size_t line_length = strcspn(line, "\n") + 1;
    if(bytes_read < line_length) line_length = bytes_read;
    line[line_length] = 0;

    lseek(fd, line_length - bytes_read, SEEK_CUR);
    return line_length;
}


LlCommand read_commands_config_file(char *filepath){
    LlCommand llc = NULL;

    //Abertura do ficheiro com a config
    int conf_fd  = open(filepath, O_RDONLY);

    char *buffer = (char *) malloc(1024);

    //Lê e processa uma linha do ficheiro config
    while(readln(conf_fd, buffer, 1024) > 0){
        if(llc != NULL)
            appendsLLC(llc,newCommand(buffer));
        else
            llc = newLLC(newCommand(buffer));
    }

    close(conf_fd);
    free(buffer);

    return llc;
}


int isTaskRunnable (LlCommand llc, LinkedListProcess process){
    char *commands[process->commandsCount];
    int times[process->commandsCount]; //Número de vezes que é pedido para executar o comando


    //Conta o número de vezes que cada comando é utilizado

    int n = 0; //Número de posicoes já ocupadas nos arrays
    for(int i = 0 ; i < process->commandsCount ; i++){
        int j;
        for(j = 0; j < n && strcmp(process->commands[i],commands[j]) != 0; j++);

        //Encontrou em commands
        if(j != n){
            times[j]++;
        }
        else{
            times[n]   = 1;
            commands[n] = process->commands[i];
            n++;
        }
    }

    //Verifica se a task não é impossivel de executar, se pode começar a ser executada ou não.
    int r = 1;
    for(int i = 0; i < n ; i++){
        Command c = getCommand(llc,commands[i]);

        if(c== NULL || times[i] > c->max)
            return -1;

        if(times[i] > c->max - c->running)
            r = 0;
    }

    return r;
}
