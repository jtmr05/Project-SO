#include "auxStructs.h"

//Comando para executar o servidor 
//./bin/sdstored etc/sdstore.conf bin/execs/


//Executado da pasta root do projeto

char *pathToConfigFile;
char *pathToCommandsExecs;
LlCommand commandList = NULL;
LinkedListProcess processPendingList = NULL;
LinkedListProcess processRunningList = NULL;
int taskCount = 0;
int terminateFlag = 0;
char *buffer;//Inicializacao do buffer


void processesTask(){
    //Remove a task da lista dos pending e adiciona na dos running
    LinkedListProcess process = removeProcessesHead(&processPendingList);
    if(processRunningList== NULL)
        processRunningList = process;
    else
        appendsProcess(processRunningList, process);
    
    //Cria um processo filho que vai tratar de processar o ficheiro
    //Exit code filho:
    //  0 caso sucesso
    //  1 caso haja erros
    pid_t pid_filho;
    if((pid_filho = fork()) == 0){
        //Abertura do ficheiro ao qual se pretende aplicar os comandos
        int input_fp = open(process->input_file, O_RDONLY);
        if(input_fp == -1) { perror("Open Input file"); _exit(1); }

        //Criacao do ficheiro no qual ficará o resultado final de aplicar os comandos
        int output_fp = open(process->output_file, O_CREAT | O_WRONLY | O_TRUNC , 0644);
        if(output_fp == -1) { perror("Open Output file"); _exit(1); }


        int commandsCount = process->commandsCount;
        int breadth;
        int pipes[commandsCount - 1][2]; //Tendo N comandos, temos N - 1 pipes
        char *commandName;

        //Gera os diversos pipes que os processos vão usar para comunicar entre eles
        for(breadth = 0; breadth < commandsCount ; breadth++){
            //Path para a pasta dos comandos
            char pathToCommand[1024]; 
            strcpy(pathToCommand, pathToCommandsExecs);

            //Path para o comando
            commandName = getCommand(commandList, process->commands[breadth])->type;

            /*
            strcat(pathToCommand, commandName);           
			*/	

            // É o primeiro comando
            if(breadth == 0){

                //Situação de apenas ter um comando
                if(commandsCount == 1){

                    if(fork() == 0){
                        //Redirecionamento do stdin para o ficheiro de input fornecido
                        dup2(input_fp, STDIN_FILENO);
                        close(input_fp);

                        //Redirecionamento do stdout para o ficheiro de output fornecido
                        dup2(output_fp, STDOUT_FILENO);
                        close(output_fp);

                        //Execucao do comando
                        execl(pathToCommand, commandName, NULL);
                    }
                }
                //É o primeiro mas existem mais comandos, logo é preciso pelo menos um pipe
                else{

                    //Cria o pipe
                    if(pipe(pipes[0]) == -1) _exit(1);

                    if(fork() == 0){
                        //Fecha o extremo de leitura do pipe
                        close(pipes[0][0]);

                        //Redirecionamento do stdin para o ficheiro de input fornecido
                        dup2(input_fp, STDIN_FILENO);
                        close(input_fp);

                        //Redirecionamento do stdout para o extremo de escrita do pipe
                        dup2(pipes[0][1], STDOUT_FILENO);

                        //Execucao do comando
                        execl(pathToCommand, commandName, NULL);
                    }
                    else{
                        //Fecha o extremo de escrita do pipe
                        close(pipes[0][1]);
                    }
                }

                close(input_fp);
            }
            //É o último comando (Apenas quando tem mais do que um comando)
            else if(breadth == commandsCount - 1){
                
                if(fork() == 0){
                    //Redirecionamento do stdin para o extremo de leitura do pipe
                    dup2(pipes[breadth - 1][0], STDIN_FILENO);
                    close(pipes[breadth - 1][0]);

                    //Redirecionamento do stdout para o ficheiro de output fornecido
                    dup2(output_fp, STDOUT_FILENO);
                    close(output_fp);

                    //Execucao do comando
                    execl(pathToCommand, commandName, NULL);
                }
                else{
                    //Fecha o extremo de escrita do pipe
                    close(pipes[breadth - 1][0]);
                    close(output_fp);
                }
            }
            //É comando intermédio
            else{
                //Cria o pipe comando
                if(pipe(pipes[breadth]) == -1) _exit(1);

                if(fork() == 0){
                    //Redirecionamento do stdin para o extremo de leitura do pipe
                    dup2(pipes[breadth - 1][0], STDIN_FILENO);
                    close(pipes[breadth - 1][0]);

                    //Redirecionamento do stdout para o extremo de escrita criado
                    dup2(pipes[breadth][1], STDOUT_FILENO);
                    close(pipes[breadth][1]);

                    //Execucao do comando
                    execl(pathToCommand, commandName, NULL);
                }
                else{
                    //Fecha o extremo de escrita do pipe
                    close(pipes[breadth - 1][0]);
                    close(pipes[breadth][1]);
                }
            }
        }

        //Espera que todos os comandos acabem de executar
        for(breadth = 0; breadth < commandsCount ; breadth++) wait(NULL);

        _exit(0);
    }
    else {
        //Coloca o pid do filho que está a processor o ficheiro na estrutura correspondente à tarefa
        process->pid_child = pid_filho;
        
        //Atualiza a tabela de comandos
        for(int i = 0 ; i < process->commandsCount ; i++){
           	Command c = getCommand(commandList, process->commands[i]);
            incRunningCommand(c);
        }

        //Sinaliza o cliente que já começou a ser processado
        kill(process->pid_client, SIGUSR1);
    }
}

void handle_sigterm(){
    terminateFlag = 1;
}

void handle_sigalrm(){

    //Verificacao dos pedidos que já acabaram, de modo a atualizar a lista dos processing
    int status = -1;

    //Enquanto houverem filhos que acabaram, vai tratar deles
    pid_t pid_filho;
    while((pid_filho = waitpid(-1, &status, WNOHANG)) > 0){        
        //Remove o processo da lista de "Running processes"
        LinkedListProcess process = removeProcessByChildPid(&processRunningList, pid_filho);

        //Informa (Sinaliza) o cliente do resultado de executar a tarefa
        if(WEXITSTATUS(status) == 0)
            kill(process->pid_client, SIGUSR2);
        else
            kill(process->pid_client, SIGTERM);

        //Atualiza a lista dos comandos
        if(process != NULL){
            for(int i = 0 ; i < process->commandsCount ; i++){
                Command c = getCommand(commandList, process->commands[i]);
                decRunningCommand(c);
            }

            freeProcess(process);
        }
    }

    //Inicio de novas tasks caso possível
    int podeProcessar = 1; //flag para indicar se pode passar a task para running / serve como condicao para manter o loop a correr enquanto houver tasks possíveis de começar a executar
    while(podeProcessar == 1){
        if(processPendingList != NULL){
            //Verifica se o primeiro elemento da lista (está a servir como uma espécie de queue), pode começar a ser processado
            podeProcessar = isTaskRunnable(commandList, processPendingList);

            //Se os ćomando pedidos para serem executados superarem o limite, então a task é eliminada, e o cliente é avisado
            if(podeProcessar == -1){
                LinkedListProcess process = removeProcessesHead(&processPendingList);
                kill(process->pid_client, SIGTERM);
                freeProcess(process);
            }
            //Caso seja possivel processar
            else if(podeProcessar == 1){
                processesTask();
            }
            //Se podeProcessar == 0, entao vai sair do ciclo  
            //e esperar que hajam comando disponiveis para aplicar à task 
            //que se encontra em primeiro na lista, ou seja, 
            //a do cliente que está mais tempo à espera
        }
        else podeProcessar = 0;
    }

    //Termina o programa caso tenha sido sinalizado com SIGTERM, e já não existam tasks para fazer
    if(terminateFlag == 1 && processRunningList == NULL && processPendingList == NULL) {
        printf("Closing server...\n");

        //Frees do buffer e da estrutura que contem os comando
        free(buffer);
        freeLLC(commandList);

        //Eliminacao dos fifos
        unlink("./tmp/client_server_fifo");
        unlink("./tmp/server_client_fifo");
        unlink("./tmp/gate_fifo");

        exit(0);
    }

    alarm(1);
}

int main(int argc, char *argv[]){
    //Verifica se o path para o config file e para os comandos é fornecido.
    if(argc != 3) {printf("Verifique os argumentos.\n"); return 0;}
    
    //Guarda os paths nas variaveis globais
    pathToConfigFile   = argv[1];
    pathToCommandsExecs = argv[2];

    //Inicializacao Variaveis
    commandList = read_commands_config_file(pathToConfigFile);

    //Inicializacao dos fifos
    if(mkfifo("./tmp/client_server_fifo", 0644) == -1) {perror("Fifo"); return -1;}
    if(mkfifo("./tmp/server_client_fifo", 0644) == -1) {perror("Fifo"); return -1;}
    if(mkfifo("./tmp/gate_fifo", 0644) == -1) {perror("Fifo"); return -1;}
    //Sinais
    signal(SIGALRM, handle_sigalrm);
    signal(SIGTERM, handle_sigterm);

    //Começa um alarme. Os alarmes vão servir para verificar os pedidos que já acabaram e começar novos
    alarm(1);

    //Inicializacao do buffer
    buffer = (char *) calloc(MESSAGE_SIZE, 1);

    while(1){

        //Abre o gate
        int gate_fd = open("./tmp/gate_fifo", O_WRONLY);        

        //Sinaliza um cliente que pode enviar informacao pelo client_server_fifo
        write(gate_fd, "B", 1);

        //Fecha o gate
        close(gate_fd);

        //Descritores
        int client_server_fd = open("./tmp/client_server_fifo", O_RDONLY),
            server_client_fd = open("./tmp/server_client_fifo", O_WRONLY);

        //Lê do client_server_fifo 
        int bytes = read(client_server_fd, buffer, MESSAGE_SIZE);

        if(bytes > 0){            
            if(strstr(buffer, "status") != NULL){
            printf("Got here \n");         
                //Obtem o pid do cliente
                pid_t pid = 0;
                sscanf(buffer, "pid: %d status", &pid);

                //Print das tasks que estão à espera de ser executadas
                write(server_client_fd, "Pending List:\n", 14);
                if(processPendingList != NULL){printf("Status");
                    for(LinkedListProcess tmp = processPendingList; tmp != NULL ; tmp = tmp->next)
                        printProcessInfo(server_client_fd, tmp);
                }
                else { write(server_client_fd, "No tasks are being performed!\n", 30); } 

                //Print das tasks a serem executadas
                write(server_client_fd, "Processing List:\n", 17);
                if(processRunningList != NULL){
                    for(LinkedListProcess tmp = processRunningList; tmp != NULL ; tmp = tmp->next)
                        printProcessInfo(server_client_fd, tmp);
                }
                else { write(server_client_fd, "No tasks are being performed!\n", 30); } 

                //Print dos comandos
                for(LlCommand tmp = commandList; tmp != NULL ; tmp = tmp->next)
                    printCommand(server_client_fd, tmp->command);

                //Sinaliza o cliente, para que este possa continuar
                kill(pid, SIGALRM);
            }
            else if(strstr(buffer, "proc-file") != NULL){
                //Se a terminateFlag fôr igual a 1 então mata os clientes, informando que já não vai receber mais pedidos
                
                if(terminateFlag == 1){
                    //Obtem o pid do cliente
                    pid_t pid = 0;
                    sscanf(buffer, "pid: %d", &pid);
                    
                    //Mata o cliente e informa que já não está a receber pedidos
                    kill(pid, SIGINT);
                }
                else{
                    //Transforma o input num pedido
                    LinkedListProcess process = parseProcess(buffer, ++taskCount);

                    //Adiciona à lista de processos em espera
                    if(processPendingList == NULL)
                        processPendingList = process;
                    else
                        appendsProcess(processPendingList, process);
                }
            }
        }

        //"Reinicia" o buffer
        memset(buffer, 0, MESSAGE_SIZE);

        //Close dos fifos
        close(client_server_fd);
        close(server_client_fd);
    }

    return 0;
}