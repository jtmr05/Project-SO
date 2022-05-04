#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

#define MESSAGE_SIZE 4096
#define STATUS 1
#define PROCESS 2

//Nao faz nada, é apenas usado para acordar o programa e permitir a continuação. Usado quando é feito o pedido do status
void handle_sigalrm(){}

void handle_sigusr1(){
    //Informa que o pedido está a ser processado
    printf("Processing...\n");

    //Pausa o processo novamente, o qual vai esperar pelo sinal 'SIGUSR2'
    pause();
}

void handle_sigusr2(){
    //Informa que o pedido acabou de ser processado
    printf("Finished.\n");

    //Termina o processo
    _exit(0);
}

void handle_sigterm(){
    //Informa que o pedido não pode ser processado
    printf("Request cannot be processed. Check arguments.\n");

    //Termina o processo
    _exit(0);
}

void handle_sigint(){
    //Informa que o servidor não aceita mais pedidos
    printf("Server is closed! No more tasks are being accepted!\nClosing client...\n");

    //Termina o processo
    _exit(0);
}

void printHelp(){
	printf("Verifique os argumentos! \nComandos disponíveis: \n-> bcompress \n-> bdecompress \n-> gcompress \n-> gdecompress \n-> encrypt \n-> decrypt \n-> nop \n");
}

int main(int argc, char *argv[]){

	//Sinais com handlers
    signal(SIGALRM, handle_sigalrm);
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);
    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigint);

	if (argc >= 2){

		int statusOrProcess;
        int curr_arg = 2;

        if(!strcmp("status", argv[curr_arg]))
            statusOrProcess = STATUS;
        else if(!strcmp("proc-file", argv[curr_arg]))
            statusOrProcess = PROCESS;
        else
            printHelp();

        ++curr_arg;

        //Abre of fifo da gate
		int gate_fd = open("./tmp/gate_fifo", O_RDONLY);

		if(gate_fd == -1) {
			perror("Erro no fifo gate");
			return -1;
		}

        //Espera receber o sinal para poder enviar dados para o servidor

        int bytes = 0;
        char buffer_gate[1] = {0};

        while((bytes = read(gate_fd,buffer_gate, 1)) > 0){
            if(buffer_gate[0] == 'B')
                break;
        }

        //Fecha o fifo da gate
        close(gate_fd);

        //Descritores
        int client_server_fd = open("./tmp/client_server_fifo", O_WRONLY);

		if(client_server_fd == -1) {
			perror("Fifo");
			close(gate_fd);
			return -1;
		}

        int server_client_fd = open("./tmp/server_client_fifo", O_RDONLY);

        if(server_client_fd == -1) {
        	perror("Fifo");
        	close(gate_fd);
        	close(client_server_fd);
        	return -1;
        }

        //Buffer que guardará a mensagem recebida do servidor
        char buffer[MESSAGE_SIZE] = {0};

	 	if (statusOrProcess == STATUS){
	 		//Criacao da string a enviar ao servidor
            snprintf(buffer, MESSAGE_SIZE, "pid: %d status", getpid());

            //Escrita no fifo de cliente para servidor
            write(client_server_fd, buffer, strlen(buffer));

            //Pausa o cliente, e espera pelo sinal de continuar e ler o status fornecido pelo servidor
            pause();

            //Caso tenha mais do que 'MESSAGE_SIZE' chars para dar print, é preciso entrar neste while
            int bytes;
            while((bytes = read(server_client_fd, buffer, MESSAGE_SIZE)) == MESSAGE_SIZE){
                memset(buffer, 0, MESSAGE_SIZE);
            }

            //Termina o processo
            close(client_server_fd);
            close(server_client_fd);
            return 0;
	 	}
	 	else if(argc >= 6){

            const int priority = atoi(argv[curr_arg++]);
            assert(priority > -1);  //garantir que prioridade está
            assert(priority < 6);   //no intervalo de 0 a 5


	 	    //Criacao da string a enviar ao servidor
            snprintf(
                buffer,
                MESSAGE_SIZE,
                "pid: %d proc-file\n%d\n%s\n%s",
                getpid(),
                priority,
                argv[curr_arg++],
                argv[curr_arg++]
            ); //argv[3] -> input file name / argv[4] -> output file name

            //Acrescenta os comandos à string
            while(curr_arg < argc) {
                strncat(buffer, "\n", MESSAGE_SIZE);
                strncat(buffer, argv[curr_arg++], MESSAGE_SIZE);
            }

            //Envio da string para o servidor
            write(client_server_fd, buffer, strlen(buffer));

            //Print da mensagem que o pedido está à espera de ser processado
            printf("Pending...\n");

            //Pausa o cliente, e espera pelo sinal 'SIGUSR1' que indicará o começo do processamento do pedido.
            pause();
	 	}
	}
	else
        printHelp();

	return 0;
}