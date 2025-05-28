
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// Filas (Queues) são utilizadas no LRU e FIFO
typedef struct queueCell queueCell;
struct queueCell {
	int block;
	queueCell *next;
};

queueCell **queueHeads;

void enqueue (int set, int block);
int dequeue (int set);
void updateLRU (int set, int block);
void printQueue (int nsets);

int main( int argc, char *argv[ ] ) {

	srand(time(NULL));

	if (argc != 7) {
		printf("Numero de argumentos incorreto. Utilize:\n");
		printf("./cache_simulator <nsets> <bsize> <assoc> <substituição> <flag_saida> arquivo_de_entrada\n");
		exit(EXIT_FAILURE);
	}

	if (strcmp(argv[4], "R") != 0 && strcmp(argv[4], "L") != 0 && strcmp(argv[4], "F") != 0) {
		printf("Política de substituição inválida\nSão permitidos: L (Least Recently Used), F (First In, First Out) e R (Random)\n");
		exit(EXIT_FAILURE);
	}

	int nsets = atoi(argv[1]);
	int bsize = atoi(argv[2]);
	int assoc = atoi(argv[3]);
	char *replace = argv[4];
	int flagOut = atoi(argv[5]);
	int cacheSize = nsets * bsize * assoc;
	char *entryFileName = argv[6];
	FILE *entryFile = fopen(entryFileName, "rb");
	int totalAvailablePositions = 0; // Controla se ainda há algum espaço não inicializado na cache

	printf("nsets = %d\n", nsets);
	printf("bsize = %d\n", bsize);
	printf("assoc = %d\n", assoc);
	printf("subst = %s\n", replace);
	printf("flagOut = %d\n", flagOut);
	printf("arquivo = %s\n", entryFileName);
	printf("\n");

    if (entryFile == NULL) {
        printf("\nArquivo não encontrado");
        exit(EXIT_FAILURE);
    }

	// Criação da fila caso a política de substituição não seja Random
	if (strcmp(replace, "R") != 0) {
		queueHeads = (queueCell **) malloc(sizeof(queueCell) * nsets);
		for (int i = 0; i < nsets; i++) {
			// Definição das cabeças
			queueHeads[i] = (queueCell *) malloc(sizeof(queueCell));
			queueHeads[i]->next = NULL;
		}
	}

	// Criação da cache para a simulação
	int ***cache;
	// Alocando conjuntos
	cache = (int ***) malloc(nsets * sizeof(int **));

	for (int i = 0; i < nsets; i++) {
		// Alocando blocos
		cache[i] = (int **) malloc(assoc * sizeof(int *));

		for (int j = 0; j < assoc; j++) {
			// Alocando número de palavras de cada blocos
			totalAvailablePositions++;
			cache[i][j] = (int *) malloc(bsize * sizeof(int));
			for (int k = 0; k < bsize; k++) {
				cache[i][j][k] = -1;
			}
		}
	}

	unsigned int number;
	int set;
	int block;
	int accesses = 0;
	int capacityMisses = 0;
	int conflictMisses = 0;
	int compulsoryMisses = 0;
	int totalHits = 0;
	int hit = 0;
	int availablePosition = -1;
	int spatialLocality = -1;
	int firstNeighborAddress = -1;
	
	// Leitura do arquivo de entrada
	while (fread(&number, 4, 1, entryFile) != 0) {
		// Conversão de Big Endian para inteiro
        number =
			(0xFF000000 & number) >> 24 |
			(0x00FF0000 & number) >> 8 |
			(0x0000FF00 & number) << 8 |
			(0x000000FF & number) << 24
		;

		set = (number / bsize) % nsets;

		// Tentar encontrar o dado
		hit = 0;
		availablePosition = -1;
		for (int i = 0; i < assoc; i++) {
			for (int j = 0; j < bsize; j++) {
				if (cache[set][i][j] == number) {
					// Hit
					totalHits++;
					block = i;
					hit = 1;
					break;
				}
				if (availablePosition == -1 && cache[set][i][j] == -1) {
					// Tenta encontrar uma posição livre para alocar o novo dado dentro do conjunto
					availablePosition = i;
				}
			}
		}

		// Miss
		if (hit == 0) {
			// Número importante para saber em qual posição do bloco o número solicitado ficará (utilizado para se explorar localidade espacial)
			spatialLocality = number / bsize;

			// Procura qual o número que deverá ocupar o primeiro endereço no bloco da cache (utilizado para se explorar localidade espacial)
			// Em caso de blocos com somente um endereço, "firstNeighborAddress" terá o mesmo valor de "number"
			firstNeighborAddress = number;
			while ((firstNeighborAddress - 1) / bsize == spatialLocality && firstNeighborAddress > 0){
				firstNeighborAddress--;
			}

			// Em caso de não haver blocos vagos para que o dado seja salvo
			if (availablePosition == -1) {
				if (strcmp(replace, "R") == 0) {
					// Random
					block = rand() % assoc;
				} else
				if (strcmp(replace, "F") == 0) {
					// FIFO
					block = dequeue(set);
					enqueue(set, block);	
				} else {
					// LRU
					block = dequeue(set);
					enqueue(set, block);
				}
			} else {
				block = availablePosition;
				if (strcmp(replace, "R") != 0) {
					enqueue(set, block);
				}
			}

			if (cache[set][block][0] == -1) {
				// Miss compulsório
				compulsoryMisses++;
				totalAvailablePositions--;
			} else
			if (totalAvailablePositions > 0) {
				// Miss de conflito
				conflictMisses++;
			}
			else {
				// Miss de capacidade
				capacityMisses++;
			}

			for (int i = 0; i < bsize; i++) {
				// Guarda os novos dados na cache
				cache[set][block][i] = firstNeighborAddress++;
			}
		} else
		if (strcmp(replace, "L") == 0) {
			// Atualiza a fila quando houver um hit no LRU (coloca esse dado no fim da fila, de modo que se torne o último a sair)
			updateLRU(set, block);
		}
		accesses++;
    }

	int totalMisses = capacityMisses + conflictMisses + compulsoryMisses;
	if (flagOut) {
		// Flag de saída padrão
		printf("%d %.4f %.4f %.2f %.2f %.2f",
			accesses,
			(float) totalHits/accesses,
			(float) totalMisses/accesses,
			(float) compulsoryMisses/totalMisses,
			(float) capacityMisses/totalMisses,
			(float) conflictMisses/totalMisses
		);
	} else {
		// Flag de saída customizado
		// Printa a cache
		printf("Estado final da cache:\n\n");
		for (int i = 0; i < nsets; i++) {
			for (int j = 0; j < assoc; j++) {
				printf("|%3d|%3d|=(", i, j);
				for (int k = 0; k < bsize; k++) {
					printf("%3d", cache[i][j][k]);
					if ( k != bsize - 1 ) {
						printf(", ");
					}
				}
				printf("); ");
			}
			printf("\n");
		}
		printf("\n");

		printf(
			"Tamanho total da cache = %d endereços\n"
			"Total de acessos = %d\n"
			"Hits = %d\n"
			"Misses compulsórios = %d\n"
			"Misses de capacidade = %d\n"
			"Misses de conflito = %d\n"
			"Misses totais = %d\n\n",
			cacheSize,
			accesses,
			totalHits,
			compulsoryMisses,
			capacityMisses,
			conflictMisses,
			totalMisses
		);
		printf(
			"Taxa de acertos = %.2f%%\n"
			"Taxa de faltas = %.2f%%\n"
			"Taxa de misses compulsórios = %.2f%%\n"
			"Taxa de misses de capacidade = %.2f%%\n"
			"Taxa de misses de conflito = %.2f%%\n",
			(float) totalHits/accesses * 100,
			(float) totalMisses/accesses * 100,
			(float) compulsoryMisses/totalMisses * 100,
			(float) capacityMisses/totalMisses * 100,
			(float) conflictMisses/totalMisses * 100
		);
	}

	// Exclui os dados alocados da memória, tanto da fila quanto da cache
	for (int i = 0; i < nsets; i++) {
		for(int j = 0; j < assoc; j++ ) {
			free(cache[i][j]);
		}
		free(cache[i]);
	}
	free(cache);

	if (strcmp(replace, "R") != 0) {
		queueCell *current = NULL;
		queueCell *currentNext = NULL;
		for (int i = 0; i < nsets; i++) {
			current = queueHeads[i];
			while (current != NULL) {
				currentNext = current->next;
				free(current);
				current = currentNext;
			}
		}
	}
	free(argv);

	fclose(entryFile);

	return 0;
}

void enqueue (int set, int block) {
	// Adiciona um índice de bloco no fim da fila
	queueCell *newCell = (queueCell *) malloc (sizeof(queueCell));
	newCell->block = block;
	newCell->next = NULL;

	queueCell *current;
	for (current = queueHeads[set]; current->next != NULL; current = current->next);
	current->next = newCell;
}

int dequeue (int set) {
	// Exclui e retorna um índice de bloco do início da fila
	queueCell *removedCell = queueHeads[set]->next;
	int valueToReplace = removedCell->block;
	queueHeads[set]->next = removedCell->next;
	free(removedCell);
	return valueToReplace;
}

void updateLRU (int set, int block) {
	// Move um índice de bloco para o fim da fila
	queueCell *current, *previous, *mostRecentModified;
	previous = queueHeads[set];
	current = queueHeads[set]->next;
	while (current->block != block) {
		previous = current;
		current = current->next;
	}
	if (current->next != NULL) {
		mostRecentModified = current;
		previous->next = current->next;
		while (current->next != NULL) current = current->next;
		current->next = mostRecentModified;
		mostRecentModified->next = NULL;
	}
	
}

void printQueue (int nsets) {
	// Printa a fila (para ser utilizada em testes)
	for (int i = 0; i < nsets; i++) {
		for(
			queueCell *current = queueHeads[i]->next;
			current != NULL;
			current = current->next
		) {
			printf("(%d)", current->block);
			if(current->next != NULL) printf(" -> ");
		}
		printf("\n");
	}
	printf("\n");
}