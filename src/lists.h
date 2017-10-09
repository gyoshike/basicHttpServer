typedef struct command_list{
	char command[128];
	struct command_list *next;
	struct param_list *params;
} command_list;

typedef struct param_list{
	char param[1028];
	struct param_list *next;
} param_list;

void getRequest(char* dest);
void getParam(char* dest);
void add_command_list(char *command);
void add_param_list_begin(char *param);
void print_list();
void printRequestInListToFile(int fileFD);
