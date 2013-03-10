#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

int main (int argc, const char * argv[]) {
	
	if (argv[1] == NULL) return 0;
	
	int fd;
	int tamanho = (strlen(argv[1]) + 1)* (sizeof(char));
	if (tamanho > 1023) {
		printf("Submicao demasiado grande.\n");
		return -1;
	}
	fd = open("/tmp/gestorFIFO",O_WRONLY);
	
	if (fd < 0) {
		if (errno == ENOENT) printf("Execute primeiro o gestor de processos antes de comecar a submeter!\n");
		return -1;
	}
	
	
	write(fd, &tamanho, sizeof(int));
	write(fd, argv[1], sizeof(char)*tamanho);
	close(fd);
	
    return 0;
}
