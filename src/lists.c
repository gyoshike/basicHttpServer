#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lists.h"

//TODO: Novas maneiras de percorrer a lista

struct command_list * list = NULL;

void add_command_list(char *command)
{
	//se a lista nao tiver sido criada, cria o primeiro elemento
	if(list == NULL){

    		list = malloc(sizeof(command_list));

    		strcpy(list->command, command);
    		list->next = NULL;
    		list->params = NULL;

    		return;
	}

	command_list * current = list;

	while (current->next != NULL) {
		current = current->next;
	}

	current->next = malloc(sizeof(command_list));
	strcpy(current->next->command, command);
	current->next->next = NULL;
	current->next->params = NULL;	

	return;
}


void add_param_list_begin(char *param)
{
	command_list * current = list;

	while (current->next != NULL) {
		current = current->next;
	}

	//se a lista de parametros nao tiver sido criada, cria o primeiro elemento
	if(current->params == NULL){

		current->params = malloc(sizeof(param_list));

		strcpy(current->params->param, param);
		current->params->next = NULL;

		return;
	}

	param_list * new_node;
	new_node = malloc(sizeof(param_list));

	strcpy(new_node->param, param);
	new_node->next = current->params;
	current->params = new_node;

	return;
}

void cleanLists() {
    command_list *auxCmdList = list;
    param_list *auxParamList = list->params;

    while(list != NULL) {
        while(list->params != NULL) {
            auxParamList = list->params->next;
            free(list->params);
            list->params = auxParamList;
        }
        auxCmdList = list->next;
        free(list);
        list = auxCmdList;
    }
    return;
}

void getRequest(char* dest) {
	strcpy(dest, list->command);
}

void getRequestPath(char* dest) {
	strcpy(dest, list->params->param);
}

//Retorna no char dest o parametro num. paramNum do comando cmdName
//Escreve nulo caso o parametro desejado nao seja encontrado
void getParam(char* dest, char* cmdName, int paramNum) {
    command_list* current = list;
    param_list* currentParam;
    int i=1;
    
    while(current != NULL) {
        if(!strcmp(current->command, cmdName)) {
            currentParam = current->params;
            while(currentParam != NULL) {
                if(i == paramNum) {
                    strcpy(dest, currentParam->param);
                    return;
                }
                currentParam = currentParam->next;
                i++;
            }
        }
        current = current->next;
    }
    strcpy(dest, NULL);
}


//Imprime a request parseada em um arquivo
void printRequestInListToFile(int fileFD) {
    char buf[1024];


    command_list* current = list;
    param_list* currentParam = list->params;
    sprintf(buf, "%s", current->command);
    write(fileFD, buf, strlen(buf));
    while(currentParam->param != NULL) {
        sprintf(buf, " %s", currentParam->param);
        write(fileFD, buf, strlen(buf));
        currentParam = currentParam->next;
    }
    
    sprintf(buf, "\r\n");
    write(fileFD, buf, strlen(buf));
    current = current->next;
    while(current != NULL) {
        sprintf(buf, "%s:", current->command);
        write(fileFD, buf, strlen(buf));
        currentParam = current->params;
        while(currentParam != NULL) {
            if(currentParam->next == NULL) sprintf(buf, "%s\r\n", current->params->param);
            else sprintf(buf, "%s,", currentParam->param);
            write(fileFD, buf, strlen(buf));
            currentParam = currentParam->next;
        }
        current = current->next;
    }
    write(fileFD, "\r\n", 2);
    return;        
}
