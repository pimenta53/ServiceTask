#include <sys/wait.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "aux.h"


#define MAX_ARG 30 // Numero maximo de argumentos por processo
#define MAX_PROC 30 // Numero maximo de processos por tarefa
#define PRINT_ERR 0 // Caso esteja a 0 não imprime informações para o stderr
#define	PRINT_COL 1 // Caso esteja a 0 não relata informação sobre colisões 

typedef struct processo {
	char* nome;
	char *list_arg[MAX_ARG];
	pid_t pid;
	int pipeInput; // local onde o processo obtem o input
	int pipeOutput; // local onde o processo obtem o output
	struct processo* next;
} *PROC;


typedef struct submicao {
	PROC proc; // lista dos processos afectos à submiçao
	char* input; // input inicial dos processos
	int id_input;
	char* output; // output final dos processos
	int id_output;
	int numProcessos;
	int pid_proc_final; //Pid do ultimo processo
	int pid_proc_inicial;
} *SUBM;


typedef struct lista_submicao {
	SUBM sub;
	int id; // Id da submicao
	int dependencia[2]; // Cada submicao pode ter no max 2 dependencias, uma de input e outra de output! Caso exista dependencias, este array contem os ids das submicoes que esta dependente! Caso esteja ok, esta tudo a 0.
	struct lista_submicao* next;
} *LISTA_SUB;


typedef struct gest_strc {
	LISTA_SUB pedidos;
	int numSubmicoesActivas;
	int numSubmicoesInactivas;
	int numSubmicoesAtendidas;
} GESTOR;

GESTOR gestor;

/* Rotina que deve ser invocada ao fechar o programa */
void rotinaFecho(int sinal) {
	
	
	signal(SIGCHLD, SIG_IGN);
	if (sinal > 0) {
		if (PRINT_ERR) fprintf(stderr,"Gestor terminado com o sinal %d\n",sinal);
	}
	// Rotina para terminar todos os filhos do gestor.
	int encontrado = 0;
	LISTA_SUB aux_lista = gestor.pedidos;
	PROC aux_proc ;
	while (aux_lista && (encontrado == 0)) {
		aux_proc = aux_lista ->sub->proc;
		while ((aux_proc) && (encontrado == 0)) {
			if ((aux_proc->pid > 0) && (processExists(aux_proc->pid) == 0)) {
				(void)kill(aux_proc->pid, SIGKILL); // Faz com que o processo termine imediatamente. SIGKILL nao pode ser apanhado nem ignorado.
			}
			
			
			aux_proc = aux_proc -> next;
		}
		aux_lista = aux_lista -> next;
	}
	
	printf("\n\nGestor Terminado\n\n");
	(void)remove("/tmp/gestorFIFO");
	if (sinal) exit(EXIT_SUCCESS); 
	else exit(EXIT_FAILURE);
}




/*
 Pega numa submição e executa-a!
 
 Ao atender a um processo existem tres tipos de casos:
 
 1 -  Caso em que é o primeiro processo (Input dado pelo utilizador ou nao);
 2 -  Processo no meio da submição (Temos de ser nos a canalizar o input e o output);
 3 -  Caso em que e o ultimo processo (Output dado pelo utilizador ou nao).
 */
void atendeSubmicao(SUBM submicao) {
	
	int res_fork = 0;
	int res_pipe = 0;
	int res_open = 0;
	
	int primeiro = 1;
	gestor.numSubmicoesActivas++;
	
	PROC proc_ant = NULL;
	PROC proc_actual = submicao ->proc;
	int fd[2];

	while (proc_actual) {
		
		
		if (proc_actual->next != NULL) { // Caso 1 ou 2
			
			res_pipe = pipe(fd);
			
			
			if (res_pipe < 0) {
				
				printf("PIPE FALHOU");
				exit(EXIT_FAILURE);
			}
			
			proc_actual->pipeOutput = fd[1];
			proc_actual->next->pipeInput = fd[0];
			
			
		} else {
			//Caso 3
			if (submicao->output != NULL) {
				res_open = submicao->id_output = open(submicao->output, O_CREAT | O_WRONLY,0644); // Estamos a criar o ficheiro de output caso nao exista
				
				if (res_open == -1) {
					printf("Erro ao tratar o ficheiro de outupt final \"%s\".\n",submicao->output);
					rotinaFecho(-1);
				}
				
				proc_actual->pipeOutput = submicao->id_output;
			} 
			
		}
		
		if (proc_ant == NULL) { // Caso 1
			if (submicao->input != NULL) {
				res_open = submicao->id_input = open(submicao->input, O_RDONLY);
				
				if (res_open == -1) {
					if (errno == ENOENT){
						printf("Ficheiro de input inicial \"%s\" inexistente!",submicao->input);
					} else {
						printf("Erro ao tratar o ficheiro de input inicial.\n");
					}
					rotinaFecho(-1);
				}
				
				proc_actual->pipeInput = submicao->id_input;
			} 
		}
		
		
		res_fork = fork();
		
		
		if (res_fork == -1) {
			
			printf("ERRO DE FORK\n");
			rotinaFecho(-1);
			
		}
		
		
		if (res_fork == 0) { 
			// Processo filho
			
			if (proc_ant == NULL) {
				// Este é o primeiro processo. Caso 1 !
				// Caso nao seja indicado ficheiro de input. Não faz nada!!!
				
				if (submicao->id_input) {
					proc_actual ->pipeInput = submicao->id_input;
					dup2(submicao->id_input,STDIN_FILENO);
				}
				
				
			} else {
				// Caso 2 ou 3
				
				
				dup2(proc_actual->pipeInput, STDIN_FILENO);
				
				
			}
			
			
			
			
			if (proc_actual->next == NULL) {
				// Este e o ultimo processo. Caso 3 !
				// Caso nao seja indicado ficheiro de output. Nao faz nada!!!
				
				
				if (submicao->id_output) {
					
					
					dup2(submicao->id_output, STDOUT_FILENO);
				}
				
			} else {
				//caso 1 ou 2
				
				
				close(proc_actual->next->pipeInput);
				dup2(proc_actual->pipeOutput,STDOUT_FILENO);
				
				
			}
			
			(void)execvp(proc_actual ->nome, proc_actual->list_arg);
			printf("Erro ao executar processo!\n");
			
			exit(EXIT_FAILURE);
			
		} else {
			//Processo pai
			
			
			close(proc_actual->pipeInput);
			close(proc_actual->pipeOutput);
			
			kill(res_fork, SIGSTOP); //Quero que os processos parem logo, para nao existirem problemas.
			if (primeiro) { submicao -> pid_proc_inicial = res_fork; primeiro = 0; }
			
			submicao -> pid_proc_final = res_fork;
			proc_actual ->pid = res_fork;
			if (PRINT_ERR) fprintf(stderr,"Filho %d nasceu.\n",res_fork);
			
			proc_ant = proc_actual;
			proc_actual = proc_actual -> next;
		}
		
		
	}
	
	PROC aux = submicao->proc;
	while (aux) { // Iniciar os processos todos
		kill(aux-> pid, SIGCONT);
		aux = aux -> next;
	}
	
}

/*
  Procura o nome de um processo através do sua id
 */
char *encontraProcesso(int pid_enc) {

	LISTA_SUB aux_lista = gestor.pedidos;
	PROC aux_proc;
	
	
	while (aux_lista) {
		aux_proc = aux_lista->sub->proc;
		while (aux_proc) {
			if (aux_proc -> pid == pid_enc){ return (aux_proc->nome);}
			aux_proc = aux_proc -> next;
		
		}
		aux_lista = aux_lista -> next;
												
	}
	
	return NULL;
}

/*
Atribui recurso libertado a um outro processo que o necessite. Tendo os mais velhos prioridade.
*/
void trataDependencias(LISTA_SUB lista, int depLibertada) {

	LISTA_SUB aux = lista;
	int encontrado = 0;
	int control = 0;
	
	
	while ((aux) && (encontrado == 0)) {
		if (aux->dependencia[0] == depLibertada) {
			control = 1;
			aux->dependencia[0] = 0;
			encontrado = 1;
		}
		
		if (aux->dependencia[1] == depLibertada) {
			control = 2;
			aux->dependencia[1] = 0;
			encontrado = 1;
		}
		
		if (!(encontrado)){ aux = aux-> next;}
	}
	

	
	if (aux != NULL) {
		if ((aux->dependencia[0] == 0) && (aux->dependencia[0] == 0)) {
			gestor.numSubmicoesInactivas--;
			atendeSubmicao((aux->sub));
		}
	}
	
	while (lista && control) {
		
			if (control == 2) {
				if (lista->dependencia[0] == depLibertada) lista->dependencia[0] = aux->sub->pid_proc_final;
				if (lista->dependencia[1] == depLibertada) lista->dependencia[1] = aux->sub->pid_proc_final;
			} else {
				if (lista->dependencia[0] == depLibertada) lista->dependencia[0] = aux->sub->pid_proc_inicial;
				if (lista->dependencia[1] == depLibertada) lista->dependencia[1] = aux->sub->pid_proc_inicial;
			}
		lista = lista->next;
	}
	
	

	
}

/*
 Faz free a uma SUBM
*/
void freeSubmicao (SUBM apagar) {

	if (apagar->input != NULL) {free(apagar->input);}
	if (apagar->output != NULL){free(apagar->output);}

	int i = 0;
	PROC proc = apagar->proc;
	PROC aux;
	while (proc) {
		aux = proc;
		proc = proc-> next;
		
		free(aux->nome);
		i = 0;
		while(aux->list_arg[i]){
			free(aux->list_arg[i]);
			i++;
		}
		free(aux);
	}
	
}


/*
 Executada sempre que uma submicao e apagada.
 */
void rotinaSubmicaoApagada (LISTA_SUB antes,LISTA_SUB acabou) {
		
	freeSubmicao(acabou->sub);
	free(acabou->sub);
	if (antes == NULL) {
		gestor.pedidos = acabou->next;
	} else {
		antes->next = acabou->next;
	}
	free(acabou);
	
	
}

/*
 Verifica se determinado pid corresponde a um dos processos da lista recebida.
 */
int procPertence(PROC proc, int pid) {

	while (proc) {
		if (proc->pid == pid) {
			proc->pid = -1;
			return 1;
		
		}
		proc = proc -> next;
	}
	
	return 0;
}

/*
 Deve ocorrer sempre que um processo filho morre.
*/
void rotinaFilhoMorreu(int val) {
	
	signal(SIGCHLD, SIG_IGN); //Para tentar garantir que a rotina nao e interrompida.
	int status;
	int pid_filho;
	
	
	if (val < 0) { //Caso em que um processo filho acabado foi apanhado pelo rogueCleaner()
		pid_filho = val * -1; 
		status = -1;
	} else {
		pid_filho = waitpid(-1,&status,WNOHANG);
	}

	while (pid_filho > 0) {
	
		
		
		char *nome_filho = encontraProcesso(pid_filho);
		int encontrado = 0;
	
	/*
	 If the exit status value (see section Program Termination) of the child process is zero, then the status value reported by waitpid or wait is also zero. 
	 @ glibc manual
	*/
		if ((status == 0) && (nome_filho != NULL)) {
			if (PRINT_ERR) fprintf(stderr,"Processo %s com o pid %d terminou com sucesso.\n",nome_filho,pid_filho);
		} else {
			if (stat > 0 ) {
				if (nome_filho) {
					if (PRINT_ERR) fprintf(stderr,"Processo %s com o pid %d nao terminou correctamente!\n",nome_filho,pid_filho);
				} else {
					if (PRINT_ERR) fprintf(stderr,"Processo com o pid %d nao terminou correctamente!\n",pid_filho); 
				}
			}
		}
	
		LISTA_SUB lista_aux = gestor.pedidos;
		LISTA_SUB lista_ant = NULL;
	
		while ((lista_aux) && (encontrado == 0)) {
			if (procPertence(lista_aux->sub->proc, pid_filho)) {
				encontrado = 1;
				lista_aux->sub->numProcessos--;
				
				if ((lista_aux->sub->pid_proc_inicial == pid_filho)) {
			
					if (lista_aux->sub->input != NULL) {
						free(lista_aux->sub->input);
						lista_aux->sub->input = NULL;
					}
					trataDependencias(lista_aux, pid_filho);
				}
			
				if ((lista_aux->sub->pid_proc_final == pid_filho)) {
					if (lista_aux->sub->output != NULL) {
						free(lista_aux->sub->output);
						lista_aux->sub->output = NULL;
					}
					trataDependencias(lista_aux, pid_filho);
				}
				
			
				if (lista_aux->sub->numProcessos == 0) {
				
					rotinaSubmicaoApagada(lista_ant, lista_aux);
					gestor.numSubmicoesActivas--;
				}
	
			} else {
				lista_ant = lista_aux;
				lista_aux = lista_aux->next;
			}
		}
	
	
		if (PRINT_ERR) fprintf(stderr,"Filho %d morreu\n",pid_filho);
		if ((gestor.numSubmicoesActivas == 0) && (gestor.numSubmicoesInactivas == 0)) {
			if (PRINT_ERR) fprintf(stderr,"Pedidos Acabados.\n");
		}
	
		pid_filho = waitpid(-1,&status,WNOHANG);
		
	}
	signal(SIGCHLD, rotinaFilhoMorreu);
}


/*
 Trata um novo pedido do utilizador. 
 Verifica se tem colisões ou se pode ser executado imediatamente.
 */
void trataPedido (SUBM submicao) {
	
	gestor.numSubmicoesAtendidas++;
	LISTA_SUB aux = gestor.pedidos;
	LISTA_SUB ant = NULL;
	LISTA_SUB novo_pedido = (LISTA_SUB)malloc(sizeof(struct lista_submicao));
	
	novo_pedido -> id = gestor.numSubmicoesAtendidas;
	novo_pedido -> dependencia[0] = 0;
	novo_pedido -> dependencia[1] = 0;
	novo_pedido -> next = NULL;
	novo_pedido -> sub = submicao;
	
	int col_input = 0;
	int col_output = 0;
	
	if (submicao->input == NULL) col_input = 1;
	if (submicao->output == NULL) col_output = 1;
	
	while (aux) {
	
		
		if ((col_input == 0) && (((aux->sub->input != NULL) && (strcmp(submicao->input,aux->sub->input) == 0)) || ((aux->sub->output != NULL) && (strcmp(submicao->input,aux->sub->output) == 0)))) {
			col_input = 1;
			if ((aux->sub->output != NULL) && (!strcmp(submicao->input,aux->sub->output))){
				if (PRINT_COL) printf("\n----Colisao detectada----\nProblema: Recurso a ser utilizado como input deste pedido esta a ser utilizado como output em outra submicao.\nResolucao: Colocar pedido em espera.\nRecurso: %s;\nPID Processo Responsavel: %d;\nID Pedido Conflituoso: %d;\n\n",submicao->input,aux->sub->pid_proc_final,novo_pedido->id);
				novo_pedido -> dependencia[0] = aux->sub->pid_proc_final;
			} else {
				if (PRINT_COL) printf("\n----Colisao detectada----\nProblema: Recurso a ser utilizado como input duplamente.\nResolucao: Colocar pedido em espera.\nRecurso: %s;\nPID Processo Responsavel: %d;\nID Pedido Conflituoso: %d;\n\n",submicao->input,aux->sub->pid_proc_inicial,novo_pedido->id);
				novo_pedido -> dependencia[0] = aux->sub->pid_proc_inicial;
			}
			
		}
		
		if ((col_output == 0) && (((aux->sub->output != NULL) && (strcmp(submicao->output,aux->sub->output) == 0)) || ((aux->sub->input != NULL) && (strcmp(submicao->output,aux->sub->input) == 0)))) { 
			col_output = 1;
			if((aux->sub->input != NULL) && (!strcmp(submicao->output,aux->sub->input))) {
				if (PRINT_COL) printf("\n----Colisao detectada----\nProblema: Recurso a ser utilizado como output deste pedido esta a ser utilizado como input em outra submicao.\nResolucao: Colocar pedido em espera.\nRecurso: %s;\nPID Processo Responsavel: %d;\nID Pedido Conflituoso: %d;\n\n",submicao->output,aux->sub->pid_proc_inicial,novo_pedido->id);
				novo_pedido -> dependencia[1] = aux->sub->pid_proc_inicial;
			} else {
				if (PRINT_COL) printf("----Colisao detectada----\nProblema: Recurso a ser utilizado como output duplamente.\nResolucao: Colocar pedido em espera.\nRecurso: %s;\nPID Processo Responsavel: %d;\nID Pedido Conflituoso: %d;\n\n",submicao->output,aux->sub->pid_proc_final,novo_pedido->id);
				novo_pedido -> dependencia[1] = aux->sub->pid_proc_final;
			}
			
		}
		
			
		ant = aux;
		aux = aux-> next;
		
	}

	if (ant == NULL) gestor.pedidos = novo_pedido; 
	else ant -> next = novo_pedido;
	
	alarm(3);

	if ((novo_pedido->dependencia[0] == 0) && (novo_pedido->dependencia[1] == 0)) {
		atendeSubmicao(novo_pedido->sub);
	
	} else {
		gestor.numSubmicoesInactivas++;
	}
}


PROC comandos(char **arg){
	
	char * pch;
	int i;
	PROC proc =NULL;
	PROC aux=NULL;
	PROC ret = NULL;
	
	while(*arg!=NULL){
		pch = strtok (*arg," ");
		proc=malloc(sizeof(struct processo));
		proc->pid = -1;
		proc->pipeInput = -1;
		proc->pipeOutput =-1;
		proc->next=NULL;
		proc->nome=malloc((strlen(pch)+1)*sizeof(char));
		strcpy(proc->nome,pch);
		i=0;
		while ((pch != NULL) && (i < (MAX_ARG-2))){
			proc->list_arg[i]=malloc((strlen(pch)+1)*sizeof(char));
			strcpy(proc->list_arg[i],pch);
			pch = strtok (NULL, " ");
			i++;
		}
		proc->list_arg[i]=NULL;
		if (aux == NULL) ret = proc;
		else aux->next = proc;
		aux = proc;
		proc = NULL;
		arg++;
	}
	return ret;
}


PROC separaComandos(char *str){
	
	char * pch;
	char *arg[MAX_PROC];
	int i=0;
	PROC processos =NULL;
	
	pch = strtok (str,"|");
	while (pch != NULL){
		arg[i]=malloc((strlen(pch)+1)*sizeof(char));
		strcpy(arg[i],pch);
		pch = strtok (NULL, "|");
		i++;
	}
	arg[i] = NULL;
	processos=comandos(arg);
	
	
	return processos;
}



/*
 Preenche os campos input e output e retorna o sitio onde estao os processos:
	1 - No inicio; (ex: "proc1 | proc2" ou "proc1 > out")
	2 - No meio; (ex: "in > proc > out")
	3 - No fim; (ex: "in > proc1 | proc2")
*/
int getInputOutput (char*str, SUBM nova_subm) {
	
	nova_subm->input = NULL;
	nova_subm->output = NULL;
	
	char *primaMaior = strtok(str, ">");
	char *duclaMaior = strtok(NULL, ">");
	char *triplaMaior = strtok(NULL, ">");
	(void)strtok(str, "|");
	char *existeParentRectos = strtok(NULL, "|");
	
	char *teste = NULL;
	
	if (existeParentRectos == str) existeParentRectos = NULL;
	

	
	if (duclaMaior != NULL) {
		if (triplaMaior != NULL) {
			//Caso em que tem dois ">". Exemplo: "num > sort > out"
			nova_subm->input = (char*)malloc(sizeof(char)*(strlen(primaMaior)+1));
			nova_subm->output = (char*)malloc(sizeof(char)*(strlen(triplaMaior)+1));
			strcpy(nova_subm->input,primaMaior);
			strcpy(nova_subm->output,triplaMaior);
			nova_subm->input = trataStringTrim(nova_subm->input);
			nova_subm->output = trataStringTrim(nova_subm->output);
			return 2;
		} else {
			//Caso em que so tem um ">". Exemplo "ls -l > output".
			if (existeParentRectos) {
				if (duclaMaior > existeParentRectos) {
					//Caso  " proc1 | proc2 > output"	
					nova_subm->input = NULL;
					nova_subm->output = (char*)malloc(sizeof(char) * (strlen(duclaMaior)+1));
					strcpy(nova_subm->output,duclaMaior);
					nova_subm->output = trataStringTrim(nova_subm->output);
					return 1;
				} else {
					//Caso  "input > proc1 | proc2"
					nova_subm->output = NULL;
					nova_subm->input = (char*)malloc(sizeof(char) * (strlen(primaMaior)+1));
					strcpy(nova_subm->input,primaMaior);
					nova_subm->input = trataStringTrim(nova_subm->input);
					return 3;
				}
			} else {
				//Caso "a > b". CASO DIFICIL DE TRATAR. Nao sabemos qual e o processo, a ou b!
				
				
				
				if (strstr(duclaMaior,".txt")) {
					nova_subm->input = NULL;
					nova_subm->output = (char*)malloc(sizeof(char)*(strlen(duclaMaior)+1));
					strcpy(nova_subm->output,duclaMaior);
					nova_subm->output = trataStringTrim(nova_subm->output);
					
					return 1;
				}
				
				
				
				if (strstr(primaMaior,".txt")) {
		
					nova_subm->output = NULL;
					nova_subm->input = (char*)malloc(sizeof(char)*(strlen(primaMaior)+1));
					strcpy(nova_subm->input,primaMaior);
					nova_subm->input = trataStringTrim(nova_subm->input);
					return 2;
				}
				
				
				teste = (char*)malloc(sizeof(char)*(strlen(primaMaior)+1));
				strcpy(teste,primaMaior);
				teste = trataStringTrim(teste);
				
							
					
				if (isProcess(teste) == 1) {
					
					nova_subm->input = NULL;
					free(teste);
					teste = (char*)malloc(sizeof(char)*(strlen(duclaMaior)+1));
					strcpy(teste,duclaMaior);
					nova_subm->output = trataStringTrim(teste);
					return 1;
				} else {
					//Assumimos que em "a > b" b e o processo.
					nova_subm->input = teste;
					nova_subm->output = NULL;
					return 2;
				}

				
			}

			
		}

	} else {
		//Caso em que nao tem ">". Exemplo: "ls"
		
		nova_subm->input = NULL;
		nova_subm->output = NULL;
		return 1;
	}
	
}

int conta_processos (PROC proc) {
	int i = 0;
	while (proc) {
		i++;
		proc = proc -> next;
	}
	return i;
}
	

SUBM parseSubmicao (char *str) {
	
	SUBM nova_subm = (SUBM)malloc(sizeof(struct submicao));
	int local_proc;
	
	
	char *subm_str = (char*)malloc(sizeof(char)*(strlen(str)+1));
	void *apt = (void*)subm_str;
	
	strcpy(subm_str,str);
	subm_str = trimString(subm_str);
	
	//Tratar do input e do output da sumbicao
	nova_subm->input = NULL;
	nova_subm->output = NULL;
	local_proc = getInputOutput(subm_str,nova_subm);
	nova_subm->id_input = 0;
	nova_subm->id_output = 0;
	
	strcpy(subm_str,str);
	subm_str = trimString(subm_str);
	
	char *proc_str = strtok(subm_str, ">");
	local_proc--;
	while (local_proc) {
		proc_str  = strtok(NULL, ">");
		local_proc--;
	}
	
	proc_str  = trimString(proc_str);
	
	//Tratar da parte dos processos da submicao
	nova_subm->proc = separaComandos(proc_str);
	nova_subm->pid_proc_final = 0;
	nova_subm->pid_proc_inicial = 0;
	nova_subm->numProcessos = conta_processos(nova_subm->proc);
	
	free(apt);
	
	return nova_subm;
}

/*
 Estabelece a comunicação com o programa submeter.
 */
int recebeSubmit () {
	
	
	int fd;
	int tamanhoParaLer = 0;
	char buff[1024];
	fd = open("/tmp/gestorFIFO",O_RDWR);
	
	if (fd == -1) {
		printf("Erro ao tratar a FIFO");
		rotinaFecho(-1);
	}
	
	while (1) { 
		read(fd, &tamanhoParaLer, sizeof(int));
		read(fd, buff, tamanhoParaLer);
		
		printf("Pedido Recebido: \"%s\" \n",buff);

		SUBM nova_submicao = parseSubmicao(buff);
		buff[0] = 0;
		trataPedido(nova_submicao);
	}
	
	return 0;
}



/*
 Cria a fifo
 */
int criaEntrada() {
	(void)remove("/tmp/gestorFIFO");
	return (mkfifo("/tmp/gestorFIFO", 0644));
}

/*
 Rotina para detectar se existe algum processo filho nao foi correctamente tratado ao sair.
 É executada regularmente de modo ao gestor nao ficar entupido com processos que por qualquer razão não foram tratados ao terminarem.
*/
void rogueCleaner () {
	
	if (gestor.numSubmicoesActivas > 0) {
		int encontrado = 0;
		LISTA_SUB aux_lista = gestor.pedidos;
		PROC aux_proc ;
		while (aux_lista && (encontrado == 0)) {
			aux_proc = aux_lista ->sub->proc;
			while ((aux_proc) && (encontrado == 0)) {
				if ((aux_proc->pid > 0) && (processExists(aux_proc->pid) == 0)) {
					// Processo que nao foi apanhado
					if (PRINT_ERR) fprintf(stderr, "Rogue Process %d apanhado.\n",aux_proc->pid);
					encontrado = 1;
					rotinaFilhoMorreu(-1 * aux_proc->pid);
				}

				
				aux_proc = aux_proc -> next;
			}
			aux_lista = aux_lista -> next;
		}
		
		alarm(1); // 1 segundo para iniciar uma nova submicao, antes que seja verificado de novo se existem mais processos rogue.
	} else {
		alarm(15);
	}

	
}

void handlerPipe() {
	
	printf("SIG PIPE ERROR.\n");
	printf("Provavelmente submicao mal feita.\n");
	rotinaFecho(0);
}


int main (int argc, const char * argv[]) {

	
	signal(SIGTERM, rotinaFecho);
	signal(SIGABRT, rotinaFecho);
	signal(SIGINT,  rotinaFecho);
	signal(SIGQUIT, rotinaFecho);
	signal(SIGUSR1, rotinaFecho);	   
	signal(SIGUSR2, rotinaFecho);
	signal(SIGHUP,	rotinaFecho);
	signal(SIGCHLD, rotinaFilhoMorreu);
	signal(SIGPIPE, handlerPipe);
	
	signal(SIGALRM, rogueCleaner);
	
	criaEntrada();
	recebeSubmit();
	
	
	rotinaFecho(-1);
	
    return 0;
}



