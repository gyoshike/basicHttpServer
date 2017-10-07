%{
#include "y.tab.h"
%}
%START		    ParametroCmd
%START		    ParametroRequest
request	        ^[A-Z]+
paramRequest	[^ \r\n]+
cmd		        ^[^: \r\n]+
paramcmd	    [^,\r\n]+
virgula         ,
doisPontos	    :
espaco		    [ ]
comentario	    #[^\n]*\r?\n
nl        	    \r?\n
%%

{nl}					{
			 		 //printf("Leu NL \n");
			 		 BEGIN 0;
			 		 return NL;
					}
{comentario}				{
			 		 //printf("Leu comentario\n");
					 BEGIN 0;
					 return COMENTARIO;
					}

<ParametroRequest>{paramRequest}	{
					 //printf("Leu reqParam %s\n", yytext);
					 sscanf(yytext, "%s", yylval.longText);
					 return PARAMREQUEST;
					}
			
<ParametroCmd>{paramcmd}		{
					 //printf("Leu cmdParam %s\n", yytext);
					 sscanf(yytext, "%1028[^\n]", yylval.longText);
					 return PARAMCMD;
					}
				
{request}				{
					 BEGIN ParametroRequest;
					 //printf("Leu request %s\n", yytext);
					 sscanf(yytext, "%s", yylval.text);
					 return REQUEST;
					}

{cmd}					{
					 //printf("Leu comando %s\n", yytext);
					 sscanf(yytext, "%s", yylval.text);
					 return CMD;
					}

{doisPontos}				{
					 BEGIN ParametroCmd;
					 //printf("Leu dois pontos\n");
					 return DOISPONTOS;
					}
{virgula}				{//printf("Leu virgula!\n");
					}
	
{espaco}				{//printf("Leu espaco!\n");
					}

%%
