#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include "aux.h"


char *trimString(char *str)
{
	char *end;
	
	
	while(isspace(*str)) str++;
	
	if(*str == 0)
		return str;
	
	end = str + strlen(str) - 1;
	while(end > str && isspace(*end)) end--;
	
	*(end+1) = 0;
	
	return str;
}


char *trataStringTrim (char *str) {
	
	void *init = str;

	str = trimString(str);
	
	char *nova = (char*)malloc(sizeof(char)*(strlen(str)+1));
	
	strcpy(nova,str);
	free(init);
	
	return nova;
}

int processExists (int pid) {

	return (kill(pid, 0) == 0); // Manda um sinal inoquo para verificar se o processo esta vivo
	
}

int isProcess(char *nome) {

	char *nomeTratado =  (char *)malloc(sizeof(char)*(strlen(nome)+1));
	void *init = nomeTratado;
	strcpy(nomeTratado,nome);
	(void)strtok(nomeTratado, " ");
	
	
	if (!(access(nomeTratado,X_OK))) {
		free(init);
		return 1;
	}
	char *teste = (char*)malloc(sizeof(char)*(strlen(nome)+25));
	
	strcpy(teste,"/bin/");
	if (!(access(strcat(teste,nomeTratado),X_OK))) {
		free(init);
		free(teste);
		return 1;
	}
	
	strcpy(teste,"/usr/bin/");
	if (!(access(strcat(teste, nomeTratado),X_OK))) {
		free(init);
		free(teste);
		return 1;
	}
	
	strcpy(teste,"/sbin/");
	if (!(access(strcat(teste, nomeTratado),X_OK))) {
		free(init);
		free(teste);
		return 1;
	}
	
	strcpy(teste,"/usr/sbin/");
	if (!(access(strcat(teste, nomeTratado),X_OK))) {
		free(init);
		free(teste);
		return 1;
	}
	
	
	free(teste);
	free(init);
	return 0;
}