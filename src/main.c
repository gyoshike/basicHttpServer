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
int checkRequestAllowed(char* request);
int getResource(char* webspace, char* resourcePath);

//Funcoes para manejar erros e gerar paginas html de erro.
void issueError(int errorCode, int outFD, int logFD);
char* createHtmlErrorPage(int errorCode);

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
const char LOG_SEPARATOR[] = "\r\n----------------------------------------\r\n\r\n";
//Strings da pagina HTML de erro.
const char HTML_BODY1[] = "<!DOCTYPE html>\r\n<html>\r\n\t<head>\r\n\t\t<title>";
const char HTML_BODY2[] = "</title>\r\n\t</head>\r\n\t<body>\r\n\t\t<font size=\"10\">";
const char HTML_BODY3[] = "</font>\r\n\t</body>\r\n</html>\r\n";

int main(int argc, char* argv[]) {
    processRequest(argv[1], argv[2], argv[3], argv[4]);
    return 0;
}

//Processa a request
void processRequest(char* webspace, char* inputPath, char* outputPath, char* logPath) {
    char request[16];
    char resourcePath[1024];
    char buf[1024];
    char contentLength[12];
    int outFD, logFD, resourceFD;
    int readSize;
    struct stat statbuf;
    
    //Abre fd do log e da saida. 
    outFD = open(outputPath, O_TRUNC|O_WRONLY|O_CREAT, 0600);
    logFD = open(logPath, O_APPEND|O_WRONLY|O_CREAT, 0600);
   
    //Obtem buffer contendo a request do arquivo especificado em inputPath
    char* req = getRequestFromFile(inputPath); //Nao esquecer de dar free apos uso
    if(req == NULL) {
        issueError(500, outFD, logFD);
        close(outFD);
        close(logFD);
        free(req);
        return;
    }

    //Chama o parser sobre o buffer, populando a lista de comandos e parâmetros
    //TODO: pegar erro yyparse e lidar com erro de BAD REQUEST (400)
 	yy_scan_string(req);
	yyparse();

    //Escreve a request parseada no log e na stdout (fd=1)
	printRequestInListToFile(logFD);
    printRequestInListToFile(1);
    
	//Puxa primeiro cmd e primeiro parametro para testar
	getRequest(request);
    getParam(resourcePath);

	//Verifica se a request recebida esta implementada e é permitida
	if(checkRequestImplemented(request)) {
	    issueError(501, outFD, logFD);
        close(outFD);
        close(logFD);
        free(req);
	    return;
	}
	if(checkRequestAllowed(request)) {
	    issueError(405, outFD, logFD);
        close(outFD);
        close(logFD);
        free(req);
	    return;
    }
    
    //Implementa requisicoes
    //Implementa GET e HEAD
    if(!strcmp(request, "GET") || !strcmp(request, "HEAD")) {
        //Tenta abrir recurso e verifica erros
        resourceFD = getResource(webspace, resourcePath);
        if(resourceFD == -1) {
            issueError(403, outFD, logFD);
            close(outFD);
            close(logFD);
            close(resourceFD);
            free(req);
            return;
        }
        else if(resourceFD == -2) {
            issueError(404, outFD, logFD);
            close(outFD);
            close(logFD);
            close(resourceFD);
            free(req);
            return;
        }
        else if(resourceFD == -3) {
            issueError(500, outFD, logFD);
            close(outFD);
            close(logFD);
            close(resourceFD);
            free(req);
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
            //TODO: Fazer content-type com a syscall system
            sprintf(buf, "Content-type: ALGO\r\n", 20);
            write(outFD, buf, strlen(buf));
            write(logFD, buf, strlen(buf));
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
        for(int i=0; REQUEST_ALLOWED[i] != 0; i++) {
            strcat(buf, REQUEST_ALLOWED[i]);
            if(REQUEST_ALLOWED[i+1] != 0) strcat(buf, ", ");
            else strcat(buf,"\r\n");
        }
        strcat(buf, "Content-Length: 0");
        write(outFD, buf, strlen(buf));
        write(logFD, buf, strlen(buf));

    }
    //Implementa TRACE
    else if(!strcmp(request, "TRACE")) {
        writeCommonHeadersToOutputAndLog(200, outFD, logFD);
        strcpy(buf, "Content-Length: ");
        sprintf(contentLength, "%d\r\n", strlen(req));
        strcat(buf, contentLength);
        write(outFD, buf, strlen(buf));
        write(outFD, "\r\n", 2);
        write(logFD, buf, strlen(buf));
        printRequestInListToFile(outFD);
    }

    write(logFD, LOG_SEPARATOR, strlen(LOG_SEPARATOR));

    close(outFD);
    close(logFD);
    close(resourceFD);
    free(req);

}

//TODO: arrumar content type
void issueError(int errorCode, int outFD, int logFD) {
    char* page;
    char buf[1024];

    writeCommonHeadersToOutputAndLog(errorCode, outFD, logFD);
    page = createHtmlErrorPage(errorCode);          //Nao esquecer de dar free
    sprintf(buf, "Content-type: HTML\r\n", 20);
    write(outFD, buf, strlen(buf));
    write(logFD, buf, strlen(buf));
    sprintf(buf, "Content-Length: %d\r\n", strlen(page)); 
    write(outFD, buf, strlen(buf));
    write(logFD, buf, strlen(buf));
    write(outFD, "\r\n", 2);
    write(outFD, page, strlen(page));
    free(page);
    return;
}

//Nao esquecer de dar free no ponteiro retornado!!!!!!
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
    }

    
    pageSize = strlen(title) + strlen(message) + strlen(HTML_BODY1) + strlen(HTML_BODY2) + strlen(HTML_BODY3);
    char* page = (char*)malloc(pageSize);
    strcpy(page, HTML_BODY1);
    strcat(page, title);
    strcat(page, HTML_BODY2);
    strcat(page, message);
    strcat(page, HTML_BODY3);
    return page;
            
}

void writeCommonHeadersToOutputAndLog(int responseCode, int outFD, int logFD) {
    time_t rawtime;
    struct tm* timeinfo;
    char buf[1024];
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

    //TODO: Pegar tipo de conexao da request e usar a mesma
    strcpy(buf, "Connection-Type: keep-alive\r\n");
    write(outFD, buf, strlen(buf));
    write(logFD, buf, strlen(buf));
}


//Tenta buscar o recurso localizado no resourcePath do webSpace
//Retorna FD no sucesso, -1 Forbidden, -2 Not found, -3 Internal Server Error
int getResource(char* webspace, char* resourcePath) {
    int resourceFD;
    char path[1024];
    char auxPath[1024];
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
