/* Servidor HTTP Basico
 * Dener Stassun Christinele
 * Guilherme Augusto Sakai Yoshike */

/* TODO:
 * -Melhorar o manejamento do erro de bad request
 * -Detectar erros em opens, writes, etc para capturar internal server error
 *      ->No momento, pode acontecer escritas em um socket inválido, resultando em segfault (broken pipe)
 * -Realizar a escrita no socket (e talvez no log) fora da função processRequest de forma centralizada para evitar erro de broken pipe
 * -Organizar/estruturar codigo*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include "lists.h"
#include "y.tab.h"

//Multiprocessos
void trataSinalFilho();
//Processa a request
void processRequest(char* webspace, char* req, int outFD, char* logPath);
void cleanupProcessRequest(int outFD, int logFD, int resourceFD, char* req);
//Geram as saidas no caso de erro
void issueError(int errorCode, int outFD, int logFD);
char* createHtmlErrorPage(int errorCode);
//Obtém uma request de um socket (getRequestFromSocket) ou de um arquivo (gerRequestFromFile)
int getRequestFromSocket(char** req, int socket, int logFD, long int tolerancia);
char* getRequestFromFile(char* path);
//Retorna o FD do recurso em path. Em caso de falha, retorna codigos de erro
//Tambem escreve em path o caminho final para o recurso (importante se request for sobre diretorio)
int getResource(char* path);
//Realiza escritas comuns a todos os casos
void writeCommonHeadersToOutputAndLog(int responseCode, int outFD, int logFD); 
//Verificam se a request solicitada foi implementada e é permitada
int checkRequestImplemented(char* request);
int checkRequestAllowed(char* request);

//Requests implementadas e permitidas. 
//OBS: DELETE marcada como implementada apenas para teste do erro Method Not Allowed
const char* const REQUEST_IMPLEMENTED[] = {"GET", "HEAD", "TRACE", "OPTIONS", "DELETE", 0};
const char* const REQUEST_ALLOWED[] = {"GET", "HEAD", "TRACE", "OPTIONS", 0};
//Strings de headers comuns
const char HTTP_VERSION[] = "HTTP/1.1";
const char SERVER_INFO[] = "Servidor HTTP ver. 0.1 de Guilherme Yoshike e Dener Stassun";
const char MSG_200[] = "200 OK";
const char MSG_400[] = "400 Bad Request";
const char MSG_403[] = "403 Forbidden";
const char MSG_404[] = "404 Not Found";
const char MSG_405[] = "405 Method Not Allowed";
const char MSG_500[] = "500 Internal Server Error";
const char MSG_501[] = "501 Not Implemented";
const char MSG_503[] = "503 Service Unavailable";
//String separadora de request no log
const char LOG_SEPARATOR[] = "\r\n----------------------------------------\r\n\r\n";
//Strings da pagina HTML de erro.
const char HTML_BODY1[] = "<!DOCTYPE html>\r\n<html>\r\n\t<head>\r\n\t\t<title>";
const char HTML_BODY2[] = "</title>\r\n\t</head>\r\n\t<body>\r\n\t\t<font size=\"10\">";
const char HTML_BODY3[] = "</font>\r\n\t</body>\r\n</html>\r\n";
//Variaveis para gerenciamento multi-processo
const int MAX_FILHOS = 5;
int NUM_FILHOS = 0;

int main(int argc, char* argv[]) {
    int soquete, soquete_msg;
    struct sockaddr_in servidor, cliente;
    int tam_endereco = sizeof(cliente);
    char msg_entrada[1024];
	char msg_connection[1024];
    char* request;
    int pid, estado;               //Variaveis gerenciamento de processo

    //Estabelece tratamento de sinal dos processo filhos
    signal(SIGCHLD, trataSinalFilho);

    //Abertura do socket
    soquete = socket(AF_INET, SOCK_STREAM, 0); // 0 indica “Use protocolo padrão”
    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(atoi(argv[3])); // htons() converte valores para representação de rede
    servidor.sin_addr.s_addr = INADDR_ANY; // qualquer endereço válido
    bind(soquete, (struct sockaddr*)&servidor, sizeof(servidor));
    listen(soquete, 5); // prepara socket para recepção e lista para receber até 5 conexões
	
	
    //Loop do servidor
    while (1) {
        do {
			printf("Aguardando conexao...\n");
            soquete_msg = accept(soquete, (struct sockaddr*)&cliente, &tam_endereco);
            printf("Recebeu conexao no socket: %d\n", soquete_msg);
        } while (soquete_msg == -1 && errno == EINTR);

        //Cria processo filho para processar request
        if(NUM_FILHOS < MAX_FILHOS) {
            NUM_FILHOS++;
            pid = fork();
            //Caso seja processo filho
            if(pid == 0) {
				while(1){
					// Aguarda request do socket por 10 segundos
					printf("aguardando request do processo: %d\n", getpid());
					if(getRequestFromSocket(&request, soquete_msg, argv[2], atoi(argv[4])) > 0){
						printf("processando request\n");
						processRequest(argv[1], request, soquete_msg, argv[2]);
						getParam(msg_connection, "Connection", 1);
						cleanLists();
					} else {
						printf("Recebeu request vazia\n");
                        close(soquete_msg);
						exit(0);
					}
					
					//printf("teste:\n%s\n\n\n", request);
					// Pega valor de connection
					//getParam(msg_connection, "Connection", 1);
					//printf("conexao:%s\n", msg_connection);
					//Processa a request, escrevendo no socket e no log as saídas.
					
					// Caso seja Connection: Close, encerra o processo filho e fecha o socket
					if (strcmp(msg_connection, " Close") == 0) {
						printf("Fechando conexao...\n");
						close(soquete_msg);
						sleep(100);
						exit(0);
					}
				}
            }
        } else {
            int logFD = open(argv[2], O_APPEND|O_WRONLY|O_CREAT, 0600);
            issueError(500, soquete_msg, logFD);
            close(logFD);
        }
    }
    
    free(request);
    return 0;
}

void trataSinalFilho() {
    int estado;

    NUM_FILHOS--;
    wait3(&estado, WNOHANG, NULL);
}

//Processa a request
void processRequest(char* webspace, char* req, int outFD, char* logPath) {
    char request[16];               //Nome da request
    char resourcePath[1024];        //Caminho para o recurso
    char fullPath[1024];            //Caminho completo para o recurso considerando o webspace
    char buf[1024];                 //Buffer de uso geral em escritas
    //int outFD = -1;                 //FD do arquivo de saída
    int logFD = -1;                 //FD do arquivo de log
    int resourceFD = -1;            //FD do recurso solicitado
    struct stat statbuf;            //Struct de informacoes de um recurso
    FILE *shellCmdBin;              //Stream para executar comandos shell
	int i;
	char msg_connection[1024];
	
	//printf("request recebida:\n%s\n\n\nFIM\n", req);
    
    //Abre fd do log e da saida. 
    //outFD = open(outputPath, O_TRUNC|O_WRONLY|O_CREAT, 0600);
    logFD = open(logPath, O_APPEND|O_WRONLY|O_CREAT, 0600);
   
    //Verifica se o buffer de request passado é nulo
    if(req == NULL) {
        issueError(500, outFD, logFD);
        //cleanupProcessRequest(outFD, logFD, -1, req);
        return;
    }

    //Chama o parser sobre o buffer, populando a lista de comandos e parâmetros
    //TODO: pegar erro yyparse e lidar com erro de BAD REQUEST (400)
 	yy_scan_string(req);
	if(yyparse()) {
	    issueError(400, outFD, logFD);
	    //cleanupProcessRequest(outFD, logFD, -1, req);
	    return;
	}

    //Escreve a request parseada no log e na stdout (fd=1)
	printRequestInListToFile(logFD);
    printRequestInListToFile(1);
    
	//Puxa request e seu primeiro parametro 
	getRequest(request);
    getRequestPath(resourcePath);

    //Forma o caminho completo para o recurso
    strcpy(fullPath, webspace);
    strcat(fullPath, resourcePath);

	//Verifica se a request recebida esta implementada e é permitida
	if(checkRequestImplemented(request)) {
	    issueError(501, outFD, logFD);
        //cleanupProcessRequest(outFD, logFD, -1, req);
	    return;
	}
	if(checkRequestAllowed(request)) {
	    issueError(405, outFD, logFD);
        //cleanupProcessRequest(outFD, logFD, -1, req);
	    return;
    }
    
    //Implementa requisicoes
    //Implementa GET e HEAD
    if(!strcmp(request, "GET") || !strcmp(request, "HEAD")) {
        //Tenta abrir recurso e verifica erros

        resourceFD = getResource(fullPath);
        if(resourceFD == -1) {
            issueError(403, outFD, logFD);
            //cleanupProcessRequest(outFD, logFD, resourceFD, req);
            return;
        }
        else if(resourceFD == -2) {
            issueError(404, outFD, logFD);
            //cleanupProcessRequest(outFD, logFD, resourceFD, req);
            return;
        }
        else if(resourceFD == -3) {
            issueError(500, outFD, logFD);
            //cleanupProcessRequest(outFD, logFD, resourceFD, req);
            return;
        }
        else {
            //Escreve headers no arquivo de saída e de log
            writeCommonHeadersToOutputAndLog(200, outFD, logFD);
            fstat(resourceFD, &statbuf);
            sprintf(buf, "Last-Modified: %s", ctime(&statbuf.st_mtime));
            write(outFD, buf, strlen(buf));
            write(logFD, buf, strlen(buf));
            sprintf(buf, "Content-Length: %lld\r\n", (long long)statbuf.st_size);
            write(outFD, buf, strlen(buf));
            write(logFD, buf, strlen(buf)); 
            //Roda o comando "file" para pegar content-type
            strcpy(buf, "Content-Type: ");
            write(outFD, buf, strlen(buf));
            write(logFD, buf, strlen(buf));
            sprintf(buf, "file -b %s", fullPath);
            if((shellCmdBin = popen(buf, "r")) == NULL) {
                issueError(500, outFD, logFD);
                //cleanupProcessRequest(outFD, logFD, resourceFD, req);
                return;
            }
            fgets(buf, 1024, shellCmdBin);
            write(outFD, buf, strlen(buf));
            write(logFD, buf, strlen(buf));
            pclose(shellCmdBin);
            if(!strcmp(request, "GET")) {
                //Escreve recurso no arquivo se saída
                write(outFD, "\r\n", 2);
                char byte;
                while(read(resourceFD, &byte, 1) != 0) {
                    write(outFD, &byte, 1);
                }
            }
        }         
    }
    //Implementa OPTIONS
    else if(!strcmp(request, "OPTIONS")) {
        writeCommonHeadersToOutputAndLog(200, outFD, logFD);
        strcpy(buf, "Allow: ");
        for(i=0; REQUEST_ALLOWED[i] != 0; i++) {
            strcat(buf, REQUEST_ALLOWED[i]);
            if(REQUEST_ALLOWED[i+1] != 0) strcat(buf, ", ");
            else strcat(buf,"\r\n");
        }
        strcat(buf, "Content-Length: 0\r\n");
        write(outFD, buf, strlen(buf));
        write(logFD, buf, strlen(buf));

    }
    //Implementa TRACE
    else if(!strcmp(request, "TRACE")) {
        writeCommonHeadersToOutputAndLog(200, outFD, logFD);
        sprintf(buf, "Content-Length: %d\r\n", strlen(req));
        write(outFD, buf, strlen(buf));
        write(logFD, buf, strlen(buf));
        sprintf(buf, "Content-Type: ASCII text\r\n");
        write(outFD, buf, strlen(buf));
        write(outFD, "\r\n", 2);
        write(logFD, buf, strlen(buf));
        printRequestInListToFile(outFD);
    }

    write(logFD, LOG_SEPARATOR, strlen(LOG_SEPARATOR));
	
    //cleanupProcessRequest(outFD, logFD, resourceFD, req);
    return;
}

void cleanupProcessRequest(int outFD, int logFD, int resourceFD, char* req) {
    if(outFD != -1) close(outFD);
    if(logFD != -1) close(logFD);
    if(resourceFD != -1) close(resourceFD);
    if(req != NULL) free(req);
    cleanLists();
    return;
}

void cleanupSocketMessage(int socket, char* req) {
    if(socket != -1) close(socket);
    //if(logFD != -1) close(logFD);
    //if(resourceFD != -1) close(resourceFD);
    if(req != NULL) free(req);
    cleanLists();
    return;
}

void issueError(int errorCode, int outFD, int logFD) {
    char* page;
    char buf[1024];

    writeCommonHeadersToOutputAndLog(errorCode, outFD, logFD);
    //Coloca no ponteiro page espaço alocado com a pagina HTML. Liberar memória após uso!
    page = createHtmlErrorPage(errorCode);        
    sprintf(buf, "Content-type: HTML document\r\n", 20);
    write(outFD, buf, strlen(buf));
    write(logFD, buf, strlen(buf));
    sprintf(buf, "Content-Length: %d\r\n", strlen(page)); 
    write(outFD, buf, strlen(buf));
    write(logFD, buf, strlen(buf));
    write(outFD, "\r\n", 2);
    write(outFD, page, strlen(page));
    write(logFD, LOG_SEPARATOR, strlen(LOG_SEPARATOR));
    free(page);
    return;
}

/* Cria pagina HTML contendo o erro especificado 
 * em errorCode e a retorna a string contendo a pagina.
 * IMPORTANTE: Após utilizar a string, desalocar espaço */
char* createHtmlErrorPage(int errorCode) {
    char title[64];
    char message[64];
    int pageSize;
    switch(errorCode) {
        case 400:
            strcpy(title, MSG_400);
            strcpy(message, MSG_400);
            break;
        case 403:
            strcpy(title, MSG_403);
            strcpy(message, MSG_403);
            break;
        case 404:
            strcpy(title, MSG_404);
            strcpy(message, MSG_404);
            break;
        case 405:
            strcpy(title, MSG_405);
            strcpy(message, MSG_405);
            break;
        case 500:
            strcpy(title, MSG_500);
            strcpy(message, MSG_500);
            break;
        case 501:
            strcpy(title, MSG_501);
            strcpy(message, MSG_501);
            break;
		case 503:
            strcpy(title, MSG_503);
            strcpy(message, "Tente novamente mais tarde");
            break;
    }

    
    pageSize = strlen(title) + strlen(message) + strlen(HTML_BODY1) + strlen(HTML_BODY2) + strlen(HTML_BODY3);
    char* page = (char*)malloc(pageSize+1);
    strcpy(page, HTML_BODY1);
    strcat(page, title);
    strcat(page, HTML_BODY2);
    strcat(page, message);
    strcat(page, HTML_BODY3);
    return page;
            
}

/* Escreve os headers comuns a todas as respostas nos arquivos
 * dados por outFD e logFD. */
void writeCommonHeadersToOutputAndLog(int responseCode, int outFD, int logFD) {
    time_t rawtime;
    struct tm* timeinfo;
    char buf[1024];

    //Forma linhas em buf e as escreve nos arquivos
    strcpy(buf, HTTP_VERSION);
    strcat(buf, " ");
    switch (responseCode) {
    case 200:
        strcat(buf, MSG_200);
        break;
    case 400:
        strcat(buf, MSG_400);
        break;
    case 403:
        strcat(buf, MSG_403);
        break;
    case 404:
        strcat(buf, MSG_404);
        break;
    case 405:
        strcat(buf, MSG_405);
        break;
    case 500:
        strcat(buf, MSG_500);
        break;
    case 501:
        strcat(buf, MSG_501);
        break;
	case 503:
        strcat(buf, MSG_503);
        break;
    }
    strcat(buf, "\r\n");
    write(outFD, buf, strlen(buf));
    write(logFD, buf, strlen(buf));

    strcpy(buf, "Date: ");
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strcat(buf, asctime(timeinfo));
    write(outFD, buf, strlen(buf));
    write(logFD, buf, strlen(buf));
 
    strcpy(buf, "Server: ");
    strcat(buf, SERVER_INFO);
    strcat(buf, "\r\n");
    write(outFD, buf, strlen(buf));
    write(logFD, buf, strlen(buf));

    //Caso seja bad request ou timeout, nao tentar pegar connection do navegador
   if(responseCode != 400 && responseCode != 500 && responseCode != 503) {
        strcpy(buf, "Connection:");
        write(outFD, buf, strlen(buf));
        write(logFD, buf, strlen(buf));
        getParam(buf, "Connection", 1);
        strcat(buf, "\r\n");
        write(outFD, buf, strlen(buf));
        write(logFD, buf, strlen(buf));	   
   }
}


/* Tenta buscar o recurso localizado no resourcePath do webSpace
 * Retorna FD no sucesso, -1 Forbidden, -2 Not found, -3 Internal Server Error */
int getResource(char* path) {
    int resourceFD;
    char auxPath[1024];
    struct stat statbuf;


    //Acessa o recurso com a chamada stat
    if (stat(path, &statbuf) == -1) {
        if (errno == EACCES) return -1;         //Erro de acesso (Diretorio probido)
        else if (errno == ENOENT) return -2;    //Arquivo nao existe
        else return -3;                         //Erro interno
    }

    //Verifica se o recurso tem permissao de leitura
    if (access(path, R_OK) != 0) return -1;      //Arquivo sem perm. de leitura


    //Caso o recurso seja arquivo
    if ((statbuf.st_mode & S_IFMT) == S_IFREG) {
        //Abre com open e retorna. Caso open der errado, internal error
        if ((resourceFD = open(path, O_RDONLY, 0600)) == -1) return -3;
        else return resourceFD;
    }

    //Caso o recurso seja diretorio
    else if ((statbuf.st_mode & S_IFMT) == S_IFDIR) {

        //Verifica se existe permissao de exec do diretorio. Caso nao, forbidden
        if (access(path, X_OK) != 0) return -1;

        //Busca index.html
        strcpy(auxPath, path);
        strcat(auxPath, "/index.html");
        if ((resourceFD = open(auxPath, O_RDONLY, 0600)) == -1) {
            if(errno == EACCES) return -1;
            else if(errno == ENOENT) {
                //Caso nao exista index.html, verifica welcome.html
                strcpy(auxPath, path);
                strcat(auxPath, "/welcome.html");
                if ((resourceFD = open(auxPath, O_RDONLY, 0600)) == -1) {
                    //Verifica erros
                    if (errno == EACCES) return -1;
                    else if (errno == ENOENT) return -2;
                    else return -3;
                }
                else {
                    strcpy(path, auxPath);
                    return resourceFD;
                }
            }
            else return -3;
        }
        else {
            strcpy(path, auxPath);
            return resourceFD;
        }        
    }
}

int getRequestFromSocket(char** req, int socket, int logFD, long int tolerancia) {
	int n, c;
	fd_set fds;
	struct timeval timeout;
	int size;
    
	size = -1;
	(*req) = (char*)malloc(4096*sizeof(char));
	//char buffer[128];
	
	FD_ZERO(&fds); 					/*Limpa o set fds que será utilizado na chamada select*/
	FD_SET(socket, &fds); 			/*Atribui o socket*/
	timeout.tv_sec = tolerancia; 	/* Atribui para a variável timeout o valor de 5 segundos*/
	timeout.tv_usec = 0;			/* Atribui para a variável timeout o valor de 0 microsegundos*/
	
	n = select(socket+1, &fds, (fd_set *)0, (fd_set *)0, &timeout);
	
	
	if(n > 0 && FD_ISSET(socket, &fds)) {
		//Le os dados e escreve no buffer
    	size = read(socket, (*req), 4096);
	} else if(n == 0) {
		printf("Sem requisicao em %ld s.\n", tolerancia);
		cleanupSocketMessage(socket, (*req));
		exit(1);
	} else { 
		perror("Erro em select():");
		exit(1);
	}

	return size;

}
/* Retorna uma string com o conteudo do arquivo especifico em path
 * IMPORTANTE: Após utilizar a string, desalocar espaço */
char* getRequestFromFile(char* path) {
    int r , i , j , sz ;
	FILE * fin;
	char* req;
	
	// Tenta abrir o arquivo especificado em path onde esta request
	if((fin = fopen(path, "r")) == NULL){
		printf("Failed to open request file");
		return NULL;
	}

	//Le os dados e escreve no buffer
	fseek(fin, 0, SEEK_END);
	sz = (int)ftell(fin);
	req = (char*)malloc(sz+1);
	fseek(fin, 0, SEEK_SET);
	fread (req, 1, sz, fin);
	req[sz] = '\0';

	//Fecha o arquivo e retorna o ponteiro para o espaço alocado
	fclose(fin);
	return req;
} 

/* Verifica se a request na string request está implementada, se sim retorna 0 */
int checkRequestImplemented(char* request) {
	int i;
	
    for(i=0; REQUEST_IMPLEMENTED[i] != 0; i++) {
        if(!strcmp(request, REQUEST_IMPLEMENTED[i])) return 0;
    }
    return 1;
}

/* Verifica se a request na string request é permitida, se sim retorna 0 */
int checkRequestAllowed(char* request) {
	int i;
	
    for(i=0; REQUEST_ALLOWED[i] != 0; i++) {
        if(!strcmp(request, REQUEST_ALLOWED[i])) return 0;
    }
    return 1;
}
