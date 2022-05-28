#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

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

	if (argc >=2){

		int statusOrProcess;
        if(!strcmp("status", argv[1])) statusOrProcess = STATUS;
        else if(!strcmp("proc-file", argv[1])) statusOrProcess = PROCESS;
        else printHelp();

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
            sprintf(buffer,"pid: %d status", (int) getpid());
            
            //Escrita no fifo de cliente para servidor
            write(client_server_fd, buffer, strlen(buffer));

            //Pausa o cliente, e espera pelo sinal de continuar e ler o status fornecido pelo servidor
            pause();

            //Caso tenha mais do que 'MESSAGE_SIZE' chars para dar print, é preciso entrar neste while
            int bytes;
            while((bytes = read(server_client_fd, buffer, MESSAGE_SIZE)) >0){
                write(1,buffer,bytes);
                if (bytes == MESSAGE_SIZE) memset(buffer, 0, MESSAGE_SIZE);
            }

            //Termina o processo
            close(client_server_fd);
            close(server_client_fd);
            return 0;
	 	}
	 	else {
	 		//Criacao da string a enviar ao servidor
            sprintf(buffer,"pid: %d proc-file\n%s\n%s", (int) getpid(), argv[2], argv[3]); //argv[2] -> input file name / argv[3] -> output file name

            //Acrescenta os comandos à string
            for(int i = 4; i < argc ; i++) {
                strcat(buffer, "\n"); 
                strcat(buffer, argv[i]); 
            }

            //Envio da string para o servidor
            write(client_server_fd, buffer, strlen(buffer));
            

            //Print da mensagem que o pedido está à espera de ser processado
            printf("Pending...\n");

            //Pausa o cliente, e espera pelo sinal 'SIGUSR1' que indicará o começo do processamento do pedido.
            pause();
	 	}       

	}
	else printHelp();
	return 0;	
}