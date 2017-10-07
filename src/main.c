#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include "lists.h"
#include "y.tab.h"


void processRequest(char* webspace, char* inputPath, char* outputPath, char* logPath);
char* getRequestFromFile(char* path);
int checkRequestImplemented(char* request);
int checkRequestAllowed:(char* request);
int getResource(char* webspace, char* resourcePath);

//Funcoes para manejar erros e gerar paginas html de erro.
void issueError(int errorCode);
void createHtmlErrorPage(char* title, char* text);

//Funcoes para realizar escritas em todos os casos
void writeInputRequestToTerm(char* inputReq);
void writeInputRequestToLog(char* inputReq, int logFD);
void writeCommonHeadersToOutputAndLog(int responseCode, int outFD, int logFD); 

//Requests implementadas e permitidas
const char* const REQUEST_IMPLEMENTED[] = {"GET", "HEAD", "TRACE", "OPTIONS", 0};
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
//String separadora de request no log
const char LOG_SEPARATOR[] = "\n----------------------------------------\n";

int main(int argc, char* argv[]) {
    processRequest(argv[1], argv[2], argv[3], argv[4]);
}

//Processa a request
//TODO: em todos os erros, dar close e free no que for necessário antes de retornar 
void processRequest(char* webspace, char* inputPath, char* outputPath, char* logPath) {
    char request[16];
    char resourcePath[1028];
    char buf[1028];
    char contentLength[12];
    int outFD, logFD, resourceFD;
    int readSize;
    struct stat statbuf;

    //Obtem buffer contendo a request do arquivo especificado em inputPath
    char* req = getRequestFromFile(inputPath); //Nao esquecer de dar free apos uso
    if(req == NULL) {
        //issueError(500);
        return;
    }

    //Chama o parser sobre o buffer, populando a lista de comandos e parâmetros
    //TODO: pegar erro yyparse e lidar com erro de BAD REQUEST (400)
 	yy_scan_string(req);
	yyparse();
    
	//Puxa primeiro cmd e primeiro parametro para testar
	returnRequest(request);
    returnParam(resourcePath);

	//Verifica se a request recebida esta implementada e é permitida
	if(checkRequestImplemented(request)) {
	    //issueError(501);
	    return;
	}
	if(checkRequestAllowed(request)) {
	    //issueError(405);
	    return;
    }
    
    //Implementa requisicoes
    //TODO: garantir que foi possível abrir
    outFD = open(outputPath, O_TRUNC|O_WRONLY|O_CREAT, 0600);
    logFD = open(logPath, O_APPEND|O_WRONLY|O_CREAT, 0600);
    writeInputRequestToTerm(req);
    writeInputRequestToLog(req, logFD);
    //Implementa GET e HEAD
    if(!strcmp(request, "GET") || !strcmp(request, "HEAD")) {
        //Tenta abrir recurso e verifica erros
        resourceFD = getResource(webspace, resourcePath);
        if(resourceFD == -1) {
           // issueError(403);
            return;
        }
        else if(resourceFD == -2) {
           // issueError(404);
            return;
        }
        else if(resourceFD == -3) {
           // issueError(500);
            return;
        }
        else {
            //Escreve headers no arquivo de saída e de log
            writeCommonHeadersToOutputAndLog(200, outFD, logFD);
            fstat(resourceFD, &statbuf);
            sprintf(buf, "Last-Modified: %s", ctime(&statbuf.st_mtime));
            write(outFD, buf, strlen(buf));
            write(logFD, buf, strlen(buf));
            sprintf(buf, "Content-Length: %lld\n", (long long)statbuf.st_size);
            write(outFD, buf, strlen(buf));
            write(logFD, buf, strlen(buf)); 
            //TODO: Fazer content-type com a syscall system
            sprintf(buf, "Content-type: ALGO\n", 20);
            write(outFD, buf, strlen(buf));
            write(logFD, buf, strlen(buf));
            //Escreve recurso no arquivo se saída
            write(outFD, "\n", 2);
            char byte;
            while(read(resourceFD, &byte, 1) != 0) {
                write(outFD, &byte, 1);
            }
            write(outFD, "\0", 1);
        }         

    }
    //Implementa OPTIONS
    else if(!strcmp(request, "OPTIONS")) {
        writeCommonHeadersToOutputAndLog(200, outFD, logFD);
        strcpy(buf, "Allow: ");
        for(int i=0; REQUEST_ALLOWED[i] != 0; i++) {
            strcat(buf, REQUEST_ALLOWED[i]);
            if(REQUEST_ALLOWED[i+1] != 0) strcat(buf, ", ");
            else strcat(buf,"\n");
        }
        strcat(buf, "Content-Length: 0");
        write(outFD, buf, strlen(buf));
        write(logFD, buf, strlen(buf));

    }
    //Implementa TRACE
    //TODO: Reconstruir da lista ao invés de usar o char req, pegar content-length baseado na reconstrução e não no char req
    else if(!strcmp(request, "TRACE")) {
        writeCommonHeadersToOutputAndLog(200, outFD, logFD);
        strcpy(buf, "Content-Length: ");
        sprintf(contentLength, "%u\n", (unsigned)strlen(req));
        strcat(buf, contentLength);
        write(outFD, buf, strlen(buf));
        write(outFD, "\n", 2);
        write(logFD, buf, strlen(buf));
        write(outFD, req, strlen(req));
    }

    close(outFD);
    close(logFD);
    close(resourceFD);
    free(req);

}

//TODO: Reconstruir request usando a arvore ao invés de utilizar o char
void writeInputRequestToTerm(char *inputReq) {
    printf("%s\n", inputReq);
}

//TODO: Talvez ler a entrada ou reconstruir da arvore e escrever ao invés de usar o char?
//TODO: Verificacao de erros write
void writeInputRequestToLog(char* inputReq, int logFD) {
    write(logFD, LOG_SEPARATOR, strlen(LOG_SEPARATOR)); //Escreve separador no log
    write(logFD, inputReq, strlen(inputReq));
    write(logFD, "\n", 2);
}

//TODO: Decidir se realmente uso chamadas POSIC (write, etc) ou ANSI C (fwrite)
//TODO: Verificao de erros write
void writeCommonHeadersToOutputAndLog(int responseCode, int outFD, int logFD) {
    time_t rawtime;
    struct tm* timeinfo;
    char buf[1028];
    int readSize;

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
    }
    strcat(buf, "\n");
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
    strcat(buf, "\n");
    write(outFD, buf, strlen(buf));
    write(logFD, buf, strlen(buf));

    //TODO: Pegar tipo de conexao da request e usar a mesma
    strcpy(buf, "Connection-Type: keep-alive\n");
    write(outFD, buf, strlen(buf));
    write(logFD, buf, strlen(buf));
}


//Tenta buscar o recurso localizado no resourcePath do webSpace
//Retorna FD no sucesso, -1 Forbidden, -2 Not found, -3 Internal Server Error
int getResource(char* webspace, char* resourcePath) {
    int resourceFD;
    char path[1028];
    char auxPath[1028];
    int readSize;
    struct stat statbuf;

    strcpy(path, webspace);
    strcat(path, resourcePath);

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
                else return resourceFD;
            }
            else return -3;
        }
        else return resourceFD;
    }
}

/* Abre a request no arquivo especificado em path;
 * Aloca um buffer com o tamanho do arquivo de request;
 * Escreve no buffer todo o conteúdo da request;
 * Retorna o ponteiro para o espaço alocado com a request
 * IMPORTANTE: Após utilizar o buffer, desalocar espaço */
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
    for(int i=0; REQUEST_IMPLEMENTED[i] != 0; i++) {
        if(!strcmp(request, REQUEST_IMPLEMENTED[i])) return 0;
    }
    return 1;
}

/* Verifica se a request na string request é permitida, se sim retorna 0 */
int checkRequestAllowed(char* request) {
    for(int i=0; REQUEST_ALLOWED[i] != 0; i++) {
        if(!strcmp(request, REQUEST_ALLOWED[i])) return 0;
    }
    return 1;
}
