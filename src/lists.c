#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lists.h"

struct command_list * list = NULL;

void returnRequest(char* dest) {
	strcpy(dest, list->command);
}

void returnParam(char* dest) {
	strcpy(dest, list->params->param);
}

void print_list() {
	command_list * current = list;
	while (current != NULL) {
		
		printf("%s\n", current->command);
		
		while(current->params != NULL){
			printf("Parâmetro: %s\n", current->params->param);
			current->params = current->params->next;
		}

		current = current->next;
	}
}

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
