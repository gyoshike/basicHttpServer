#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "lists.h"
#include "y.tab.h"

void returnMessage(int responseCode, int fd, struct stat statbuf, char* request);

int main(int argc, char* argv[]) {

    int fd;
    char path[128];
    char auxPath[128];
    int readSize;
    struct stat statbuf;
    
    char request[16];
    char param[1028];
    
    //começo outro arquivo
    int r , i , j , sz ;
	char *req;
	FILE * fin;

	/* argv[1] -> Arquivo contendo a requisicao */
	if((fin = fopen(argv[2], "r")) == NULL){
		printf("Error to open file %s.\n", request);
		exit (0);
	}

	/* Leitura de dados e escrita no buffer */
	fseek(fin, 0, SEEK_END);
	sz = (int) ftell(fin);
	req = ( char *) malloc(sz+1);
	fseek(fin, 0, SEEK_SET);
	fread (req, 1, sz, fin);
	req[sz] = '\0';

	/* Processando a requisicao no buffer "req" usando o parser */
	yy_scan_string(req);
	yyparse();

	/* Fechando o arquivo... */
	close(fin);
	//fim do outro arquivo

	//Puxa primeiro cmd e primeiro parametro para testar
	returnRequest(request);
    returnParam(param);

    //Verifica se argumentos foram passados corretamente
    /*if (argc < 2 && argc > 4) {
        printf("Especifique no minimo um e no maximo três argumentos\n");
        return 0;
    }*/

    //Verifica se a request foi implementada
    if (strcmp(request, "GET") && strcmp(request, "HEAD") && strcmp(request, "TRACE") && strcmp(request, "OPTIONS")) {
        returnMessage(501, -1, statbuf, argv[1]);
        return 0;
    }

    //Caso seja request de TRACE e OPTIONS, verifica se esta bem formada
    //e já retorna as informações
    //if (!strcmp(argv[1], "TRACE")) {
    if (!strcmp(request, "TRACE")) {
        //if (argc > 2) {
        //    returnMessage(400, -1, statbuf, argv[1]);
        //    return 0;
        //}
        //else {
		returnMessage(200, -1, statbuf, request);
        return 0;
    }

    else if (!strcmp(request, "OPTIONS")) {
        //if (argc > 2) {
        //    returnMessage(400, -1, statbuf, list->command);
        //    return 0;
        //}
        //else {
		returnMessage(200, -1, statbuf, request);
        return 0;
    }

    //Caso seja GET ou HEAD, continua para abertura dos arquivos

    //Combina o caminho do espaço web com o recurso solicitado
    strcpy(path, argv[1]);
    strcat(path, param);
    printf("%s\n", path);

    //Acessa o recurso com a chamada stat
    if (stat(path, &statbuf) == -1) {
        //Verifica se o erro foi de acesso (ou seja, diretorio sem permissao de execucao)
        if (errno == EACCES) {
            returnMessage(403, -1, statbuf, request);
            return 0;
        }
        //Verifica se o erro foi de arquivo/diretorio nao existente
        if (errno == ENOENT) {
            returnMessage(404, -1, statbuf, request);
            return 0;
        }
    }

    //Verifica se o recurso tem permissao de leitura
    if (access(path, R_OK) != 0) {
        returnMessage(403, -1, statbuf, request);
        return 0;
    }

    //Caso o recurso seja arquivo
    if ((statbuf.st_mode & S_IFMT) == S_IFREG) {

        //Abre o arquivo com chamada open
        if ((fd = open(path, O_RDONLY, 0600)) == -1) {
            returnMessage(403, -1, statbuf, request);
            return 0;
        }

        returnMessage(200, fd, statbuf, request);
        return 0;
    }

    //Caso o recurso seja diretorio
    else if ((statbuf.st_mode & S_IFMT) == S_IFDIR) {

        //Verifica se existe permissao de exec do diretorio
        if (access(path, X_OK) != 0) {
            returnMessage(403, -1, statbuf, request);
            return 0;
        }

        //Verifica se foi possivel abrir index.html
        strcpy(auxPath, path);
        strcat(auxPath, "index.html");
        if ((fd = open(auxPath, O_RDONLY, 0600)) == -1) {
            int auxErrNo = errno;
            //Caso nao foi possivel abrir index.html, verifica welcome.html
            strcpy(auxPath, path);
            strcat(auxPath, "welcome.html");
            if ((fd = open(auxPath, O_RDONLY, 0600)) == -1) {
                //Verifica se o erro foi de acesso (ou seja, arquivo sem permissao de leitura)
                if (errno == EACCES) {
                    returnMessage(403, -1, statbuf, request);
                    return 0;
                }
                //Verifica se o erro foi de arquivo/diretorio nao existente
                if (errno == ENOENT) {
                    //Caso o erro em index.html tenha sido de permissao
                    if (auxErrNo = EACCES) {
                        returnMessage(403, -1, statbuf, request);
                        return 0;
                    }
                    //Caso o erro em index.html tenha sido de nao existencia
                    else {
                        returnMessage(403, -1, statbuf, request);
                        return 0;
                    }
                }
            }
        }

        //Pega os stats do arquivo index.html ou welcome.html
        fstat(fd, &statbuf);
        returnMessage(200, fd, statbuf, request);
    }
    return 0;
}

void returnMessage(int responseCode, int fd, struct stat statbuf, char* request)
{
    time_t rawtime;
    struct tm* timeinfo;
    char buf[128];
    int readSize;

    printf("HTTP/1.1 ");
    switch (responseCode) {
    case 200:
        printf("200 OK\n");
        break;
    case 400:
        printf("400 Bad Request\n");
        break;
    case 403:
        printf("403 Forbidden\n");
        break;
    case 404:
        printf("404 Not Found\n");
        break;
    case 501:
        printf("501 Not Implemented\n");
        break;
    }

    //Imprime data da resposta
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    printf("Date: %s", asctime(timeinfo));

    //Imprime tipo de servidor
    printf("Server: Servidor HTTP ver. 0.1 de Guilherme Yoshike e Dener Christinele\n");

    //Imprime procedimento com a conexão
    printf("Connection: keep-alive\n");

    if (responseCode == 200) {
        //Caso a requisição seja de GET ou HEAD
        if (!strcmp(request, "GET") || !strcmp(request, "HEAD")) {

            // Data da ultima modificacao
            printf("Last-Modified: %s", ctime(&statbuf.st_mtime));
            // Tamanho do recurso (em bytes)
            printf("Content-Length: %lld\n", (long long)statbuf.st_size);
            //Imprime tipo do recurso
            printf("Content-Type: Content-Type: text/html\n");

            //Cao seja head, retorna antes de imprimir o corpo
            if (!strcmp(request, "HEAD"))
                return;

            //Imprime corpo da mensagem (recurso)
            printf("\n");
            readSize = read(fd, buf, sizeof(buf));
            while (readSize > 0) {
                write(1, buf, readSize);
                readSize = read(fd, buf, sizeof(buf));
            }
        }

        //Caso a requisição seja de TRACE
        else if (!strcmp(request, "TRACE")) {
            return;
        }

        //Caso a requisição seja de OPTIONS
        else if (!strcmp(request, "OPTIONS")) {
            printf("Allow: GET, HEAD, TRACE, OPTIONS\n");
        }
    }
}
