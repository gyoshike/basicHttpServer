%{
#include <stdio.h>
#include <stdlib.h>
#include "lists.h"
%}
%union {
        char 	text[128];
	char	longText[1024];
       }
%token <text> REQUEST 
%token <longText> PARAMREQUEST
%token <text> CMD
%token <longText> PARAMCMD
%token DOISPONTOS
%token COMENTARIO
%token NL

%%
linhas:   	linha
		| linhas linha
linha:		requestLine
		| COMENTARIO
		| cmdLine
requestLine:	REQUEST PARAMREQUEST PARAMREQUEST NL {add_command_list($1); add_param_list_begin($3); add_param_list_begin($2);}
comando:	CMD {add_command_list($1);}
parametros:	PARAMCMD parametros {add_param_list_begin($1);}
	  	| PARAMCMD NL {add_param_list_begin($1);}
		| NL
cmdLine:	comando DOISPONTOS parametros

%%
