#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/t2fs.h"
#include "../include/apidisk.h"
#include "../include/bitmap2.h"

#define MAXOPEN 20
#define DEBUG 0

int inicializa_estruturas();
void carrega_superbloco();

int isOpen(int handler);
int getFreeOpenIndex();

int carrega_entrada(struct t2fs_record* record, unsigned int bloco, unsigned int offset);
int salva_entrada(struct t2fs_record record, unsigned int bloco);
int salva_diretorio(struct t2fs_record diretorio, unsigned int bloco);

int loadFile(char* filename, struct t2fs_record* file);
int loadBottomFolder(char* filename, struct t2fs_record* file);
int getSectorFor(struct t2fs_record record, int offset);
int writeBlockTo(struct t2fs_record* record, int block);
int freeLastBlock(struct t2fs_record* record);
int freeBlocksUntil(int offset, struct t2fs_record* record);
int readFromFile(struct t2fs_record record, int initialOffset, int size, char *buffer);
int writeToFile(struct t2fs_record* record, int initialOffset, int size, char *buffer);

int cleanFolderPathName(char* filename, char* cleanedFilename);
int searchDir(struct t2fs_record dir, char* name, struct t2fs_record *file);
int getNameOfDir(struct t2fs_record dir, char* buffer, int size);

struct t2fs_superbloco *superBlock;
struct t2fs_record *root, *workingDir;

int handlerCounter = 0;
FILE2 abertosHandler[MAXOPEN];
struct t2fs_record abertosRecord[MAXOPEN];
int abertosDirBlock[MAXOPEN];
int abertosPointer[MAXOPEN];

int inicializa_estruturas() {

    carrega_superbloco();

    workingDir = malloc(sizeof(struct t2fs_record));
    carrega_entrada(workingDir, (superBlock->bitmapSize + superBlock->superBlockSize), 0);

    root = malloc(sizeof(struct t2fs_record));
    *root = *workingDir;

    int i;
    for (i = 0; i < MAXOPEN; i++) {
        abertosHandler[i] = -1;
        abertosPointer[i] = -1;
        abertosDirBlock[i] = -1;
    }
	return 0;
}

void carrega_superbloco() {

    superBlock = malloc(sizeof(struct t2fs_superbloco));

    char buffer[SECTOR_SIZE];
    if (read_sector(0, buffer) != 0)
        return;

    int index = 0;
    // id
    strcpy(superBlock->id, buffer);
    superBlock->id[4] = 0;
    index += 4;

    // versão
    superBlock->version = buffer[index] | buffer[index+1] << 8;
    index += 2;

    // blocos do superbloco
    superBlock->superBlockSize = buffer[index] | buffer[index+1] << 8;
    index += 2;

    // número de setores que formam um bloco
    superBlock->blockSize = buffer[index] | buffer[index+1] << 8;
    index += 2;

    // tamanho do disco, em blocos
    superBlock->diskSize = buffer[index] | buffer[index+1] << 8
                    | buffer[index+2] << 16 | buffer[index+3] << 24;
    index += 4;

    // número de setores lógicos
    superBlock->nOfSectors = buffer[index] | buffer[index+1] << 8
                    | buffer[index+2] << 16 | buffer[index+3] << 24;
    index += 4;

    // número de blocos lógicos para o bitmap
    superBlock->bitmapSize = buffer[index] | buffer[index+1] << 8
                    | buffer[index+2] << 16 | buffer[index+3] << 24;
    index += 4;

    // número de blocos para a raiz
    superBlock->rootSize = buffer[index] | buffer[index+1] << 8
                    | buffer[index+2] << 16 | buffer[index+3] << 24;
    index += 4;

    // número de entradas no diretório raiz
    superBlock->nOfDirEntries = buffer[index] | buffer[index+1] << 8
                    | buffer[index+2] << 16 | buffer[index+3] << 24;
    index += 4;

    // número de bytes num registro
    superBlock->fileEntrySize = buffer[index] | buffer[index+1] << 8
                    | buffer[index+2] << 16 | buffer[index+3] << 24;
    index += 4;

}

int isOpen(int handler) {
    int i;
    for (i = 0; i < MAXOPEN; i++)
        if (abertosHandler[i] == handler)
            return i;
    return -1;
}

int getFreeOpenIndex() {

    int free = -1;

    int i;
    for (i = 0; i < MAXOPEN; i++)
        if (abertosHandler[i] == -1) {
            free = i;
            break;
        }

    return free;
}

int carrega_entrada(struct t2fs_record* record, unsigned int bloco, unsigned int offset) {

    char buffer[SECTOR_SIZE];
    if (read_sector(bloco*superBlock->blockSize + (offset / 4), buffer) != 0)
        return -1;

//    int i = 0;
//    printf("\nSector %d: ", bloco*superBlock->blockSize + (offset / 4));
//    for (; i < SECTOR_SIZE; i++)
//        printf("%d ", buffer[i]);
//    printf("\n");

    // converte offset para bytes
    int index = (offset % 4) * 64;

    record->TypeVal = buffer[index];
    index++;

    if (record->TypeVal == TYPEVAL_INVALIDO)
        return -1;

    strcpy(record->name, buffer+index);
    index += 32;

    // Pula os 7 reservados
    index += 7;

    record->blocksFileSize = (buffer[index] & 0x000000FF) | (buffer[index+1] << 8 & 0x0000FF00)
                    | (buffer[index+2] << 16 & 0x00FF0000) | (buffer[index+3] << 24 & 0xFF000000);
    index += 4;

    record->bytesFileSize = (buffer[index] & 0x000000FF) | (buffer[index+1] << 8 & 0x0000FF00)
                    | (buffer[index+2] << 16 & 0x00FF0000) | (buffer[index+3] << 24 & 0xFF000000);
    index += 4;

    record->dataPtr[0] = (buffer[index] & 0x000000FF) | (buffer[index+1] << 8 & 0x0000FF00)
                    | (buffer[index+2] << 16 & 0x00FF0000) | (buffer[index+3] << 24 & 0xFF000000);
    index += 4;

    record->dataPtr[1] = (buffer[index] & 0x000000FF) | (buffer[index+1] << 8 & 0x0000FF00)
                    | (buffer[index+2] << 16 & 0x00FF0000) | (buffer[index+3] << 24 & 0xFF000000);
    index += 4;

    record->singleIndPtr = (buffer[index] & 0x000000FF) | (buffer[index+1] << 8 & 0x0000FF00)
                    | (buffer[index+2] << 16 & 0x00FF0000) | (buffer[index+3] << 24 & 0xFF000000);
    index += 4;

    record->doubleIndPtr = (buffer[index] & 0x000000FF) | (buffer[index+1] << 8 & 0x0000FF00)
                    | (buffer[index+2] << 16 & 0x00FF0000) | (buffer[index+3] << 24 & 0xFF000000);
    index += 4;

    return 0;
}

int salva_entrada(struct t2fs_record record, unsigned int bloco) {

    struct t2fs_record* temp = malloc(sizeof(struct t2fs_record));
    int i = 0;
    do {
        carrega_entrada(temp, bloco, i++);
    } while ((temp->TypeVal == 1 || temp->TypeVal == 2)
             && i < 16 && temp->dataPtr[0] != record.dataPtr[0]);

    free(temp);
    if (i == 16) {
        if (DEBUG)
            printf("\ntodas entradas ocupadas");
        return -1;
    }

    int offset = i-1;

    char buffer[SECTOR_SIZE];
    if (read_sector(bloco*superBlock->blockSize + (offset / 4), buffer) != 0) {
        if (DEBUG)
            printf("\nerro na leitura");
        return -1;
    }

    // converte offset para bytes
    int index = (offset % 4) * 64;

    buffer[index] = record.TypeVal;
    index++;

    strcpy(buffer+index, record.name);
    index += 32;

    // Pula os 7 reservados
    index += 7;

    buffer[index++] = record.blocksFileSize & 0xFF;
    buffer[index++] = (record.blocksFileSize >> 8) & 0xFF;
    buffer[index++] = (record.blocksFileSize >> 16) & 0xFF;
    buffer[index++] = (record.blocksFileSize >> 24) & 0xFF;

    buffer[index++] = record.bytesFileSize & 0xFF;
    buffer[index++] = (record.bytesFileSize >> 8) & 0xFF;
    buffer[index++] = (record.bytesFileSize >> 16) & 0xFF;
    buffer[index++] = (record.bytesFileSize >> 24) & 0xFF;

    buffer[index++] = record.dataPtr[0] & 0xFF;
    buffer[index++] = (record.dataPtr[0] >> 8) & 0xFF;
    buffer[index++] = (record.dataPtr[0] >> 16) & 0xFF;
    buffer[index++] = (record.dataPtr[0] >> 24) & 0xFF;

    buffer[index++] = record.dataPtr[1] & 0xFF;
    buffer[index++] = (record.dataPtr[1] >> 8) & 0xFF;
    buffer[index++] = (record.dataPtr[1] >> 16) & 0xFF;
    buffer[index++] = (record.dataPtr[1] >> 24) & 0xFF;

    buffer[index++] = record.singleIndPtr & 0xFF;
    buffer[index++] = (record.singleIndPtr >> 8) & 0xFF;
    buffer[index++] = (record.singleIndPtr >> 16) & 0xFF;
    buffer[index++] = (record.singleIndPtr >> 24) & 0xFF;

    buffer[index++] = record.doubleIndPtr & 0xFF;
    buffer[index++] = (record.doubleIndPtr >> 8) & 0xFF;
    buffer[index++] = (record.doubleIndPtr >> 16) & 0xFF;
    buffer[index++] = (record.doubleIndPtr >> 24) & 0xFF;

    if (write_sector(bloco*superBlock->blockSize + (offset / 4), buffer) != 0) {
        if (DEBUG)
            printf("\nerro na escrita");
        return -1;
    }

    if (isBlockFree2(record.dataPtr[0]))
        if(!allocBlock2(record.dataPtr[0])) {
            if (DEBUG)
                printf("\nerro na alocacao");
            return -1;
        }

    if (DEBUG)
        printf("\nSalvando entrada \"%s\" no offset %d do bloco %d", record.name, offset, bloco);

    return 0;
}

int salva_diretorio(struct t2fs_record diretorio, unsigned int bloco) {

    if (salva_entrada(diretorio, bloco) == -1)
        return -1;

    struct t2fs_record* self = malloc(sizeof(struct t2fs_record));
    self->TypeVal = TYPEVAL_DIRETORIO;
    strcpy(self->name, ".");
    self->blocksFileSize = 1;
    self->bytesFileSize = diretorio.bytesFileSize;

    self->dataPtr[0] = diretorio.dataPtr[0];
    self->dataPtr[1] = 0;
    self->singleIndPtr = 0;
    self->doubleIndPtr = 0;

    if (salva_entrada(*self, diretorio.dataPtr[0]) == -1) {
        free(self);
        return -1;
    }

    struct t2fs_record* parent = malloc(sizeof(struct t2fs_record));
    parent->TypeVal = TYPEVAL_DIRETORIO;
    strcpy(parent->name, "..");
    parent->blocksFileSize = 1;
    parent->bytesFileSize = diretorio.bytesFileSize;

    parent->dataPtr[0] = bloco;
    parent->dataPtr[1] = 0;
    parent->singleIndPtr = 0;
    parent->doubleIndPtr = 0;

    if (salva_entrada(*parent, diretorio.dataPtr[0]) == -1) {
        free(parent);
        free(self);
        return -1;
    }

    free(parent);
    free(self);
    return 0;
}

int loadFile(char* filename, struct t2fs_record* file) {

    char *start = filename;
    char *end;

    if (start[0] == '/') {
        *file = *root;
        start++;
    } else {
        *file = *workingDir;
        if (DEBUG)
            printf("\nPrimeiro caractere é \"%c\"", start[0]);
    }

    if (strlen(start) == 0)
        return 0;

    if (DEBUG)
        printf("\nVou tentar carregar diretório com nome \"%s\"", start);

    char *curName = malloc(sizeof(char)*strlen(filename));
    do {
        end = strchr(start, '/');
        if (end == NULL)
            end = start + strlen(start);

        strncpy(curName, start, end-start);
        curName[end-start] = '\0';

        if (searchDir(*file, curName, file) == -1) {
            free(curName);
            return -1;
        } else
            start = end+1;
    } while (end != filename + strlen(filename));

    free(curName);
    return 0;
}

int loadBottomFolder(char* filename, struct t2fs_record* file) {

    char* actualFilename = filename;
    while (strchr(actualFilename, '/') != NULL)
        actualFilename = strchr(actualFilename, '/')+1;

    if (actualFilename - filename == 0) {
        if (DEBUG)
            printf("\nO arquivo informado não possui diretório subjacente, então retornarei workingDir");
        *file = *workingDir;
        return 0;
    } else  if (actualFilename-filename == 1) { // Significa que é a raiz
        *file = *root;
        return 0;
    } else {
        char* folderName = malloc(sizeof(char)*(actualFilename-filename));
        strncpy(folderName, filename, actualFilename-filename);
        folderName[actualFilename-filename] = 0;

        cleanFolderPathName(folderName, folderName);

        if (DEBUG)
            printf("\nO arquivo informado possui diretório com nome \"%s\"", folderName);

        int ret = loadFile(folderName, file);
        free(folderName);
        return ret;
    }
}

int getSectorFor(struct t2fs_record record, int offset) {

    int blockSizeBytes = SECTOR_SIZE*superBlock->blockSize;
    int sectorToRead;

    // O primeiro bloco possui n bytes
    if (offset < blockSizeBytes) {
        sectorToRead = record.dataPtr[0]*superBlock->blockSize + (offset)/SECTOR_SIZE;
        if (DEBUG)
            printf("\nO setor é o primeiro direto (%d)", sectorToRead);
    // O segundo bloco possui mais n bytes
    } else if (offset < 2*blockSizeBytes) {
        if (record.dataPtr[1] == 0)
            return -1;
        sectorToRead = record.dataPtr[1]*superBlock->blockSize + (offset - blockSizeBytes)/SECTOR_SIZE;
        if (DEBUG)
            printf("\nO setor é o segundo direto (%d)", sectorToRead);
    // O bloco indireto endereça n/4 blocos, cada um com n bytes
    } else if (offset < 2*blockSizeBytes + blockSizeBytes*blockSizeBytes/4) {
        if (record.singleIndPtr == 0)
            return -1;

        int pointerNumber = (offset - 2*blockSizeBytes)/blockSizeBytes;
        // Cada pointeiro é 4 bytes, cabendo 64 num setor de 256 bytes
        int pointerSector = record.singleIndPtr*superBlock->blockSize + pointerNumber/(SECTOR_SIZE/4);
        int index = 4*(pointerNumber % (SECTOR_SIZE/4));

        char pointerTemp[SECTOR_SIZE];
        if (read_sector(pointerSector, pointerTemp) != 0)
            return -1;

        int block = (pointerTemp[index] & 0x000000FF) | (pointerTemp[index+1] << 8 & 0x0000FF00)
                    | (pointerTemp[index+2] << 16 & 0x00FF0000) | (pointerTemp[index+3] << 24 & 0xFF000000);

        if (block == 0)
            return -1;

        sectorToRead = block*superBlock->blockSize + ((offset) % blockSizeBytes) / SECTOR_SIZE;
    // qualquer valor maior é duas indireções
    } else if (offset < 2*blockSizeBytes + (blockSizeBytes/4 + 1)*(blockSizeBytes*blockSizeBytes/4)) {
        if (record.doubleIndPtr == 0)
            return -1;

        int secondPointerNumber = (offset - (2 + blockSizeBytes/4)*blockSizeBytes)/blockSizeBytes;
        int firstPointerNumber = secondPointerNumber / (blockSizeBytes/4);
        // Cada pointeiro é 4 bytes, cabendo 64 num setor de 256 bytes
        int firstPointerSector = record.doubleIndPtr*superBlock->blockSize + firstPointerNumber/(SECTOR_SIZE/4);
        int index = 4*(firstPointerNumber % (SECTOR_SIZE/4));

        char pointerTemp[SECTOR_SIZE];
        if (read_sector(firstPointerSector, pointerTemp) != 0)
            return -1;

        int secondPointerBlock = (pointerTemp[index] & 0x000000FF) | (pointerTemp[index+1] << 8 & 0x0000FF00)
                    | (pointerTemp[index+2] << 16 & 0x00FF0000) | (pointerTemp[index+3] << 24 & 0xFF000000);

        if (secondPointerBlock == 0)
            return -1;

        secondPointerNumber = secondPointerNumber % (blockSizeBytes/4);
        int secondPointerSector = secondPointerBlock*superBlock->blockSize + secondPointerNumber/(SECTOR_SIZE/4);
        index = 4*(secondPointerNumber % (SECTOR_SIZE/4));

        char secondPointerTemp[SECTOR_SIZE];
        if (read_sector(secondPointerSector, secondPointerTemp) != 0)
            return -1;

        int block = (secondPointerTemp[index] & 0x000000FF) | (secondPointerTemp[index+1] << 8 & 0x0000FF00)
                    | (secondPointerTemp[index+2] << 16 & 0x00FF0000) | (secondPointerTemp[index+3] << 24 & 0xFF000000);

        if (block == 0)
            return -1;

        sectorToRead = block*superBlock->blockSize + ((offset) % blockSizeBytes) / SECTOR_SIZE;
    // tentou ler depois do tamanho maximo do arquivo
    } else
        return -1;

    return sectorToRead;

}

int writeBlockTo(struct t2fs_record* record, int block) {

    int writtenBlocks = record->blocksFileSize;
    int blockSizeBytes = SECTOR_SIZE*superBlock->blockSize;

    if (writtenBlocks == 1) {
        record->dataPtr[1] = block;
        if (allocBlock2(block))
            return 0;
        else
            return -1;
    }

    // O arquivo ainda não terminou de usar o singleIndPtr
    if (writtenBlocks < 2 + blockSizeBytes/4) {
        // O arquivo nem começou a usar o singleIndPtr
        if (writtenBlocks == 2) {
            if (!allocBlock2(block)) // Aloca para não pegar o mesmo, será liberado depois
                return -1;
            int freeBlock = searchFreeBlock2();
            if (!freeBlock2(block) || freeBlock == 0 || !allocBlock2(freeBlock))
                return -1;
            // Salva o bloco do singleIndPtr
            record->singleIndPtr = freeBlock;

            // Limpa o bloco do singleIndPtr
            char temp[SECTOR_SIZE];
            int i;
            for (i = 0; i < SECTOR_SIZE; i++)
                temp[i] = 0;
            for (i = 0; i < superBlock->blockSize; i++)
                if (write_sector(record->singleIndPtr*superBlock->blockSize+i, temp) == -1)
                    return -1;
        }

        int offset = 4*(writtenBlocks - 2);
        // Adiciona o ponteiro no bloco de ponteiros
        char temp[SECTOR_SIZE];
        if (read_sector(record->singleIndPtr*superBlock->blockSize + offset/SECTOR_SIZE, temp) != 0)
            return -1;

        int index = offset % SECTOR_SIZE;
        temp[index] = block & 0xFF;
        temp[index+1] = (block >> 8) & 0xFF;
        temp[index+2] = (block >> 16) & 0xFF;
        temp[index+3] = (block >> 24) & 0xFF;

        if (write_sector(record->singleIndPtr*superBlock->blockSize + offset/SECTOR_SIZE, temp) == -1)
            return -1;

        if (allocBlock2(block))
            return 0;
        else
            return -1;

    }

    // O arquivo ainda não terminou de usar o doubleIndPtr
    if (writtenBlocks < 2 + blockSizeBytes*blockSizeBytes/16) {
        // Nem começou a usar
        if (writtenBlocks == 2 + blockSizeBytes/4) {
            if (!allocBlock2(block)) // Aloca para não pegar o mesmo, será liberado depois
                return -1;
            int freeBlock = searchFreeBlock2();
            if (!freeBlock2(block) || freeBlock == 0)
                return -1;
            // Salva o bloco do doubleIndPtr
            record->doubleIndPtr = freeBlock;

            // Limpa o bloco do doubleIndPtr
            char temp[SECTOR_SIZE];
            int i;
            for (i = 0; i < SECTOR_SIZE; i++)
                temp[i] = 0;
            for (i = 0; i < superBlock->blockSize; i++)
                if (write_sector(record->doubleIndPtr*superBlock->blockSize+i, temp) == -1)
                    return -1;

            if (!allocBlock2(freeBlock))
                return -1;
        }

        int internalOffset = 4*(writtenBlocks - 2 - blockSizeBytes/4);
        int externalOffset = 4*(internalOffset / blockSizeBytes);
        internalOffset = internalOffset % blockSizeBytes;

        // Significa que precisa alocar uma nova página de índices
        if (internalOffset == 0) {
            if (!allocBlock2(block)) // Aloca para não pegar o mesmo, será liberado depois
                return -1;
            int freeBlock = searchFreeBlock2();
            if (!freeBlock2(block) || freeBlock == 0)
                return -1;

            // Limpa o bloco que vai ser alocado
            char temp[SECTOR_SIZE];
            int i;
            for (i = 0; i < SECTOR_SIZE; i++)
                temp[i] = 0;
            for (i = 0; i < superBlock->blockSize; i++)
                if (write_sector(freeBlock*superBlock->blockSize+i, temp) == -1)
                    return -1;

            if (read_sector(record->doubleIndPtr*superBlock->blockSize+externalOffset/SECTOR_SIZE, temp) == -1)
                return -1;

            int index = externalOffset % SECTOR_SIZE;
            temp[index] = freeBlock & 0xFF;
            temp[index+1] = (freeBlock >> 8) & 0xFF;
            temp[index+2] = (freeBlock >> 16) & 0xFF;
            temp[index+3] = (freeBlock >> 24) & 0xFF;

            if (write_sector(record->doubleIndPtr*superBlock->blockSize+externalOffset/SECTOR_SIZE, temp) == -1)
                return -1;

            if (!allocBlock2(freeBlock))
                return -1;
        }
        // Não precisa alocar (ou já alocou) nova página de índices

        // Descobre o bloco da página de índices
        char temp[SECTOR_SIZE];
        if (read_sector(record->doubleIndPtr*superBlock->blockSize+externalOffset/SECTOR_SIZE, temp) == -1)
            return -1;

        int pointerBlock = (temp[externalOffset] & 0x000000FF) | (temp[externalOffset+1] << 8 & 0x0000FF00)
                    | (temp[externalOffset+2] << 16 & 0x00FF0000) | (temp[externalOffset+3] << 24 & 0xFF000000);

        // Carrega o bloco de índices e salva o novo bloco alocado
        if (read_sector(pointerBlock*superBlock->blockSize+internalOffset/SECTOR_SIZE, temp) == -1)
            return -1;

        int index = internalOffset % SECTOR_SIZE;
        temp[index] = block & 0xFF;
        temp[index+1] = (block >> 8) & 0xFF;
        temp[index+2] = (block >> 16) & 0xFF;
        temp[index+3] = (block >> 24) & 0xFF;

        if (write_sector(pointerBlock*superBlock->blockSize+internalOffset/SECTOR_SIZE, temp) == -1)
            return -1;

        if (!allocBlock2(block))
            return -1;
        else
            return 0;
    }

    // Já tem o tamanho máximo
    return -1;

}

int freeLastBlock(struct t2fs_record* record) {

    int blockSizeBytes = SECTOR_SIZE*superBlock->blockSize;
    int fileBlockCount = record->blocksFileSize-1;

    if (fileBlockCount == 0) {
        if (!freeBlock2(record->dataPtr[0]))
            return -1;
        record->dataPtr[0] = 0;
        return --record->blocksFileSize;
    }

    if (fileBlockCount == 1) {
        if (!freeBlock2(record->dataPtr[1]))
            return -1;
        record->dataPtr[1] = 0;
        return --record->blocksFileSize;
    }

    if (fileBlockCount < 2 + blockSizeBytes/4) {

        int offset = 4*(fileBlockCount - 2);
		if (DEBUG)
	        printf("\n o último bloco está nos indiretos simples, no offset %d", offset);
        char temp[SECTOR_SIZE];
        if (read_sector(record->singleIndPtr*superBlock->blockSize+offset/SECTOR_SIZE, temp) == -1)
            return -1;

        int index = offset % SECTOR_SIZE;
        int blockToRemove = (temp[index] & 0x000000FF) | (temp[index+1] << 8 & 0x0000FF00)
                    | (temp[index+2] << 16 & 0x00FF0000) | (temp[index+3] << 24 & 0xFF000000);

		if (DEBUG)
	        printf("\nli o bloco a remover, e eh %d", blockToRemove);
        temp[index] = 0;
        temp[index+1] = 0;
        temp[index+2] = 0;
        temp[index+3] = 0;

        if (write_sector(record->singleIndPtr*superBlock->blockSize+offset/SECTOR_SIZE, temp) == -1)
            return -1;

         if (!freeBlock2(blockToRemove))
            return -1;

		if (DEBUG)
	        printf("\n removi do bloco %d", record->singleIndPtr);

        if (offset == 0) { // Eliminou o último dos indiretos
            if (!freeBlock2(record->singleIndPtr))
                return -1;
            record->singleIndPtr = 0;
			if (DEBUG)
	        	printf("\nterminaram os indiretos simples, entao removi ele tambem");
        }

		if (DEBUG)
			printf("\nirei retornar %d", record->blocksFileSize-1);
        record->blocksFileSize -= 1;
        return record->blocksFileSize;
    }

    // O último usa o doubleIndPtr

    int internalOffset = 4*(fileBlockCount - 2 - fileBlockCount/4);
    int externalOffset = internalOffset / blockSizeBytes;
    internalOffset = internalOffset % blockSizeBytes;

    char externalTemp[SECTOR_SIZE];
    if (read_sector(record->doubleIndPtr*superBlock->blockSize+externalOffset/SECTOR_SIZE, externalTemp) == -1)
        return -1;

    int externalIndex = externalOffset % SECTOR_SIZE;
    int internalBlock = (externalTemp[externalIndex] & 0x000000FF) | (externalTemp[externalIndex+1] << 8 & 0x0000FF00)
                | (externalTemp[externalIndex+2] << 16 & 0x00FF0000) | (externalTemp[externalIndex+3] << 24 & 0xFF000000);

    char internalTemp[SECTOR_SIZE];
    if (read_sector(internalBlock*superBlock->blockSize+internalOffset/SECTOR_SIZE, internalTemp) == -1)
        return -1;

    int internalIndex = internalOffset % SECTOR_SIZE;
    int blockToRemove = (internalTemp[internalIndex] & 0x000000FF) | (internalTemp[internalIndex+1] << 8 & 0x0000FF00)
                | (internalTemp[internalIndex+2] << 16 & 0x00FF0000) | (internalTemp[internalIndex+3] << 24 & 0xFF000000);

    internalTemp[internalIndex] = 0;
    internalTemp[internalIndex+1] = 0;
    internalTemp[internalIndex+2] = 0;
    internalTemp[internalIndex+3] = 0;

    if (write_sector(internalBlock*superBlock->blockSize+internalOffset/SECTOR_SIZE, internalTemp) == -1)
        return -1;

     if (!freeBlock2(blockToRemove))
        return -1;

    if (internalOffset == 0) { // Eliminou o último do segundo nível
        externalTemp[externalIndex] = 0;
        externalTemp[externalIndex+1] = 0;
        externalTemp[externalIndex+2] = 0;
        externalTemp[externalIndex+3] = 0;

        if (write_sector(record->doubleIndPtr*superBlock->blockSize+externalOffset/SECTOR_SIZE, externalTemp) == -1)
            return -1;

        if (!freeBlock2(internalBlock))
            return -1;

        if (externalOffset == 0) {
            if (!freeBlock2(record->doubleIndPtr))
                return -1;
            record->doubleIndPtr = 0;
        }
    }
    return --record->blocksFileSize;
}

int freeBlocksUntil(int offset, struct t2fs_record* record) {

    int blockSizeBytes = SECTOR_SIZE*superBlock->blockSize;

    int lastBlock = offset / blockSizeBytes+1;

    while (record->blocksFileSize > lastBlock)
        if (freeLastBlock(record) == -1)
            return -1;

    return 0;
}

int readFromFile(struct t2fs_record record, int initialOffset, int size, char *buffer) {

    int read = 0;
    int sectorToRead;
    int sectorOffset;
    while (read < size && (initialOffset + read) < record.bytesFileSize) {

        sectorOffset = (initialOffset + read) % SECTOR_SIZE;
        sectorToRead = getSectorFor(record, initialOffset + read);

        if (sectorToRead == -1)
            return read;

        char temp[SECTOR_SIZE];
        if (read_sector(sectorToRead, temp) != 0) {
			if (DEBUG)
	            printf("\nerro na hora de ler o sector %d", sectorToRead);
            return -1;
        }
        while (sectorOffset < 256 && read < size && (initialOffset + read) < record.bytesFileSize)
            buffer[read++] = temp[sectorOffset++];

    }

    return read;

}

int writeToFile(struct t2fs_record* record, int initialOffset, int size, char *buffer) {

    int written = 0;
    int sectorToWrite;
    int sectorOffset;
    while (written < size) {

        sectorOffset = (initialOffset + written) % SECTOR_SIZE;
        if (DEBUG)
            printf("\ntentarei pegar o setor para o offset %d", initialOffset + written);
        sectorToWrite = getSectorFor(*record, initialOffset + written);
        if (DEBUG)
            printf("\nele me disse que é o setor %u", sectorToWrite);

        if (sectorToWrite == -1) {
            if (DEBUG)
                printf("\nTerminaram os blocos do arquivo %s", record->name);
            int freeBlock = searchFreeBlock2();
            if (freeBlock == 0)
                return written;
            if (DEBUG)
                printf("\nTerminaram os blocos do arquivo %s, mas achei um livre em %d", record->name, freeBlock);
            if (writeBlockTo(record, freeBlock) == -1)
                return written;
            record->blocksFileSize += 1;

            char temp[SECTOR_SIZE];
            int i;
            for (i = 0; i < SECTOR_SIZE; i++)
                temp[i] = 0;
            for (i = 0; i < superBlock->blockSize; i++)
                if (write_sector(freeBlock*superBlock->blockSize+i, temp) == -1) {
					if (DEBUG)
	                    printf("\nerro na hora de escrever o sector %d", freeBlock*superBlock->blockSize+i);
                    return -1;
                }

            sectorToWrite = freeBlock*superBlock->blockSize;
        }

        char temp[SECTOR_SIZE];
        if (read_sector(sectorToWrite, temp) != 0) {
			if (DEBUG)
	            printf("\nerro na hora de ler o sector %d", sectorToWrite);
            return -1;
        }

        while (sectorOffset < 256 && written < size)
            temp[sectorOffset++] = buffer[written++];

        if (write_sector(sectorToWrite, temp) != 0)
            return -1;
    }

    return written;
}

int cleanFolderPathName(char* filename, char* cleanedFilename) {

    int len = strlen(filename);
    if (*(filename + strlen(filename) - 1) == '/')
        len--;
    strncpy(cleanedFilename, filename, strlen(filename));
    cleanedFilename[len] = '\0';

    return 0;
}

int searchDir(struct t2fs_record dir, char* name, struct t2fs_record *file) {

    if (dir.TypeVal != TYPEVAL_DIRETORIO)
        return -1;

    // diretórios só possuem um bloco, então ignoro os outros ponteiros
    int block1 = dir.dataPtr[0];

    int i = 0;
    do {
        if (carrega_entrada(file, block1, i) == -1);
            //break;

        //imprime_entrada(*file);

        if (strcmp(file->name, name) == 0)
            return 0;

        i++;
    } while (i < 16); // existem 16 entradas por bloco

    return -1;
}

int getNameOfDir(struct t2fs_record dir, char* buffer, int size) {

    if (dir.dataPtr[0] == root->dataPtr[0]) {
        if (size < strlen("/"))
            return -1;
        strcpy(buffer, "/");
        return 0;
    }

    if (strcmp(dir.name, "..") == 0) {
        struct t2fs_record *parent = malloc(sizeof(struct t2fs_record));
        if (carrega_entrada(parent, dir.dataPtr[0], 1) == -1) {
            free(parent);
            return -1;
        }
        struct t2fs_record *grandparent = malloc(sizeof(struct t2fs_record));
        if (carrega_entrada(grandparent, parent->dataPtr[0], 1) == -1) {
            free(parent);
            free(grandparent);
            return -1;
        }
        int i = 0;
        do {
            carrega_entrada(parent, grandparent->dataPtr[0], i++);
        } while (parent->dataPtr[0] != dir.dataPtr[0] && i < 16);
        free(grandparent);

        if (size < strlen(parent->name)) {
            free(parent);
            return -1;
        }
        strcpy(buffer, parent->name);
        free(parent);
        return 0;
    }

    if (strcmp(workingDir->name, ".") == 0) {
        struct t2fs_record *parent = malloc(sizeof(struct t2fs_record));
        if (carrega_entrada(parent, dir.dataPtr[0], 1) == -1) {
            free(parent);
            return -1;
        }
        struct t2fs_record *self = malloc(sizeof(struct t2fs_record));
        int i = 0;
        do {
            carrega_entrada(self, parent->dataPtr[0], i++);
        } while (self->dataPtr[0] != dir.dataPtr[0] && i < 16);
        free(parent);

        if (size < strlen(parent->name)) {
            free(self);
            return -1;
        }
        strcpy(buffer, self->name);
        free(self);
        return 0;
    }

    if (size < strlen(dir.name))
        return -1;
    strcpy(buffer, dir.name);
    return 0;
}

// ===================================
// =  Implementação da biblioteca    =
// ===================================

int identify2(char *name, int size) {
    if (superBlock == NULL)
        inicializa_estruturas();

    strncpy(name, "Arthur Giesel Vedana - 2442238", size);
    name[size-1] = 0;
    return 0;
}

FILE2 open2(char* filename) {
    if (superBlock == NULL)
        inicializa_estruturas();

    int index = getFreeOpenIndex();
    if (index == -1)
        return -1;

    struct t2fs_record *file = malloc(sizeof(struct t2fs_record));
    if (loadFile(filename, file) == 0 && file->TypeVal == TYPEVAL_REGULAR) {
        abertosRecord[index] = *file;
        abertosHandler[index] = handlerCounter++;
        abertosPointer[index] = 0;

        struct t2fs_record *dir = malloc(sizeof(struct t2fs_record));
        if (loadBottomFolder(filename, dir) == -1)  {
            free(dir);
            free(file);
            return -1;
        }

        abertosDirBlock[index] = dir->dataPtr[0];

        free(dir);
        free(file);
        return abertosHandler[index];
    } else {
        free(file);
        return -1;
    }
}

FILE2 create2 (char *filename) {
    if (superBlock == NULL)
        inicializa_estruturas();

    int index = getFreeOpenIndex();
    if (index == -1)
        return -1;

    struct t2fs_record *file = malloc(sizeof(struct t2fs_record));
    if (loadFile(filename, file) == 0) {
        if (DEBUG)
            printf("\nja existe com nome");
        free(file);
        return -1;
    }

    struct t2fs_record *dir = malloc(sizeof(struct t2fs_record));
    if (loadBottomFolder(filename, dir) == -1) {
        if (DEBUG)
            printf("\ndiretorio nao existe");
        free(dir);
        free(file);
        return -1;
    }

    char* actualFilename = filename;
    while (strchr(actualFilename, '/') != NULL)
            actualFilename = strchr(actualFilename, '/')+1;

    if (strlen(actualFilename) == 0) {
        if (DEBUG)
            printf("\nNome vazio não é permitido");
        free(dir);
        free(file);
        return -1;
    }

    strcpy(file->name, actualFilename);

    file->TypeVal = TYPEVAL_REGULAR;
    file->blocksFileSize = 1;
    file->bytesFileSize = 0;

    int initialBlock = searchFreeBlock2();
    if (initialBlock == 0) {
		if (DEBUG)
	        printf("\nnao existe bloco livre");
        free(dir);
        free(file);
        return -1;
    }

    file->dataPtr[0] = initialBlock;
    file->dataPtr[1] = 0;
    file->singleIndPtr = 0;
    file->doubleIndPtr = 0;

    if (salva_entrada(*file, dir->dataPtr[0]) == -1) {
		if (DEBUG)
	        printf("\nnerro no salvamento");
        free(dir);
        free(file);
        return -1;
    }

    abertosRecord[index] = *file;
    abertosHandler[index] = handlerCounter++;
    abertosPointer[index] = 0;
    abertosDirBlock[index] = dir->dataPtr[0];

    free(dir);
    free(file);

    return abertosHandler[index];
}

int delete2(char *filename) {
    if (superBlock == NULL)
        inicializa_estruturas();

    struct t2fs_record *file = malloc(sizeof(struct t2fs_record));
    if (loadFile(filename, file) == -1 || file->TypeVal != TYPEVAL_REGULAR) {
		if (DEBUG)
	        printf("\nnao existe um arquivo com esse nome");
        free(file);
        return -1;
    }

    struct t2fs_record *dir = malloc(sizeof(struct t2fs_record));
    if (loadBottomFolder(filename, dir) == -1) {
		if (DEBUG)
        	printf("\ndiretorio nao existe");
        free(dir);
        free(file);
        return -1;
    }

    file->TypeVal = TYPEVAL_INVALIDO;
    if (salva_entrada(*file, dir->dataPtr[0]) == -1) {
        free(dir);
        free(file);
        return -1;
    }

    while (file->blocksFileSize > 0)
        if (freeLastBlock(file) == -1) {
            free(dir);
            free(file);
            return -1;
        }

    free(dir);
    free(file);
    return 0;
}

int close2(FILE2 handle) {
    if (superBlock == NULL)
        inicializa_estruturas();

    int index = isOpen(handle);
    if (handle == -1 || index == -1)
        return -1;

    struct t2fs_record* file = &abertosRecord[index];
    file->bytesFileSize = abertosPointer[index];

    if (freeBlocksUntil(file->bytesFileSize, file) == -1)
        return -1;

    if (salva_entrada(*file, abertosDirBlock[index]) == -1)
        return -1;

    abertosHandler[index] = -1;
    abertosPointer[index] = -1;
    abertosDirBlock[index] = -1;
    return 0;
}

int read2(FILE2 handle, char *buffer, int size) {
    if (superBlock == NULL)
        inicializa_estruturas();

    int index = isOpen(handle);
    if (handle == -1 || index == -1)
        return -1;

    int offset = abertosPointer[index];

    int read = readFromFile(abertosRecord[index], offset, size, buffer);

    abertosPointer[index] += read;

    return read;
}

int write2 (FILE2 handle, char *buffer, int size) {
    if (superBlock == NULL)
        inicializa_estruturas();

    int index = isOpen(handle);
    if (handle == -1 || index == -1)
        return -1;

    int offset = abertosPointer[index];

    int escritos = writeToFile(&abertosRecord[index], offset, size, buffer);

    if (offset + escritos > abertosRecord[index].bytesFileSize)
        abertosRecord[index].bytesFileSize = offset + escritos;

    salva_entrada(abertosRecord[index], abertosDirBlock[index]);
    abertosPointer[index] += escritos;

    return escritos;
}

int seek2(FILE2 handle, DWORD offset) {
    if (superBlock == NULL)
        inicializa_estruturas();

    int index = isOpen(handle);
    if (handle == -1 || index == -1)
        return -1;

    struct t2fs_record arquivo = abertosRecord[index];

    if (offset == -1)
        offset = arquivo.bytesFileSize;


    if (offset > arquivo.bytesFileSize)
        return -1;

    abertosPointer[index] = offset;

    return 0;
}

int mkdir2(char *pathname) {
    if (superBlock == NULL)
        inicializa_estruturas();

    char* cleanPathName = malloc(sizeof(char)*strlen(pathname));
    cleanFolderPathName(pathname, cleanPathName);

    if (DEBUG)
        printf("\nclean name is\"%s\"", cleanPathName);

    struct t2fs_record *folder = malloc(sizeof(struct t2fs_record));
    if (loadFile(cleanPathName, folder) == 0) {
        if (DEBUG)
            printf("\nJá existe o diretório");
        free(folder);
        free(cleanPathName);
        return -1;
    }

    struct t2fs_record *parent = malloc(sizeof(struct t2fs_record));
    if (loadBottomFolder(cleanPathName, parent) == -1) {
        if (DEBUG)
            printf("\nNão foi possível carregar o pai");
        free(parent);
        free(folder);
        free(cleanPathName);
        return -1;
    }

    char* actualFilename = cleanPathName;
    while (strchr(actualFilename, '/') != NULL)
            actualFilename = strchr(actualFilename, '/')+1;
    strcpy(folder->name, actualFilename);

    free(cleanPathName);

    folder->TypeVal = TYPEVAL_DIRETORIO;
    folder->blocksFileSize = 1;
    folder->bytesFileSize = superBlock->blockSize*SECTOR_SIZE;

    int initialBlock = searchFreeBlock2();
    if (initialBlock == 0) {
        if (DEBUG)
            printf("\nNão existem blocos livres");
        free(folder);
        free(parent);
        return -1;
    }

    folder->dataPtr[0] = initialBlock;
    folder->dataPtr[1] = 0;
    folder->singleIndPtr = 0;
    folder->doubleIndPtr = 0;

    if (salva_diretorio(*folder, parent->dataPtr[0]) == -1) {
        if (DEBUG)
            printf("\nErro na hora de salvar o diretório");
        free(parent);
        free(folder);
        return -1;
    }

    free(folder);
    free(parent);

    return 0;

}

int rmdir2(char *pathname) {
    if (superBlock == NULL)
        inicializa_estruturas();

    char* cleanPathName = malloc(sizeof(char)*strlen(pathname));
    cleanFolderPathName(pathname, cleanPathName);

    struct t2fs_record *folder = malloc(sizeof(struct t2fs_record));
    if (loadFile(cleanPathName, folder) == -1 || folder->TypeVal != TYPEVAL_DIRETORIO) {
        free(folder);
        free(cleanPathName);
        return -1;
    }

    struct t2fs_record *parent = malloc(sizeof(struct t2fs_record));
    if (loadBottomFolder(cleanPathName, parent) == -1) {
        free(parent);
        free(folder);
        free(cleanPathName);
        return -1;
    }

    char* actualFilename = cleanPathName;
    while (strchr(actualFilename, '/') != NULL)
            actualFilename = strchr(actualFilename, '/')+1;

    if (strcmp(actualFilename, ".") == 0 || strcmp(actualFilename, "..") == 0) {
        free(parent);
        free(folder);
        free(cleanPathName);
        return -1;
    }
    free(cleanPathName);

    struct t2fs_record* child = malloc(sizeof(struct t2fs_record));
    int i;
    for (i = 2; i < 16; i++)
        if (carrega_entrada(child, folder->dataPtr[0], i) == 0) {
            free(parent);
            free(folder);
            free(child);
            return -1;
        }
    free(child);

    folder->TypeVal = TYPEVAL_INVALIDO;
    if (salva_diretorio(*folder, parent->dataPtr[0]) == -1) {
        free(parent);
        free(folder);
        return -1;
    }
    free(parent);

    if (!freeBlock2(folder->dataPtr[0])) {
        free(folder);
        return -1;
    }

    free(folder);
    return 0;
}

DIR2 opendir2(char *pathname) {
    if (superBlock == NULL)
        inicializa_estruturas();

    char* cleanPathName = malloc(sizeof(char)*strlen(pathname));
    cleanFolderPathName(pathname, cleanPathName);

    int index = getFreeOpenIndex();
    if (index == -1) {
        free(cleanPathName);
        return -1;
    }

    struct t2fs_record *folder = malloc(sizeof(struct t2fs_record));
    if (loadFile(cleanPathName, folder) == 0 && folder->TypeVal == TYPEVAL_DIRETORIO) {
        abertosRecord[index] = *folder;
        abertosHandler[index] = handlerCounter++;
        abertosPointer[index] = 0;
    }

    free(cleanPathName);
    free(folder);
    return abertosHandler[index];
}

int readdir2(DIR2 handle, DIRENT2 *dentry) {
    if (superBlock == NULL)
        inicializa_estruturas();

    int index = isOpen(handle);
    if (handle == -1 || index == -1)
        return -1;

    int offset = abertosPointer[index];

    struct t2fs_record* entry = malloc(sizeof(struct t2fs_record));
    int loaded;
    do {
        loaded = carrega_entrada(entry, abertosRecord[index].dataPtr[0], offset++);
    } while (loaded == -1 && offset < 16);

    if (loaded == -1) {
        free(entry);
        return -1;
    }

    strcpy(dentry->name, entry->name);
    dentry->fileType = entry->TypeVal;
    dentry->fileSize = entry->bytesFileSize;

    abertosPointer[index] += 1;

    return 0;
}

int closedir2(DIR2 handle) {
    if (superBlock == NULL)
        inicializa_estruturas();

    int index = isOpen(handle);
    if (handle == -1 || index == -1)
        return -1;

    abertosHandler[index] = -1;
    abertosPointer[index] = -1;
    abertosDirBlock[index] = -1;
    return 0;
}

int chdir2(char *pathname) {
    if (superBlock == NULL)
        inicializa_estruturas();

    char* cleanPathName = malloc(sizeof(char)*(strlen(pathname)));
    cleanFolderPathName(pathname, cleanPathName);

    struct t2fs_record *folder = malloc(sizeof(struct t2fs_record));
    if (loadFile(cleanPathName, folder) == -1 || folder->TypeVal != TYPEVAL_DIRETORIO) {
        free(folder);
        free(cleanPathName);
        return -1;
    }
    *workingDir = *folder;

    free(cleanPathName);
    free(folder);
    return 0;
}

int getcwd2(char *pathname, int size) {
    if (superBlock == NULL)
        inicializa_estruturas();

    struct t2fs_record* temp = malloc(sizeof(struct t2fs_record));
    *temp = *workingDir;
    char* nameBuffer = malloc(sizeof(char)*size);

    if (temp->dataPtr[0] == root->dataPtr[0])
        strcpy(pathname, "/");
    else
        strcpy(pathname, "");

    while (temp->dataPtr[0] != root->dataPtr[0]) {
        getNameOfDir(*temp, nameBuffer, size);
        carrega_entrada(temp, temp->dataPtr[0], 1);

        char* buffer2 = malloc(sizeof(char)*size);
        strcpy(buffer2, "/");
        strcat(buffer2, nameBuffer);

        strcat(buffer2, pathname);
        strcpy(pathname, buffer2);
    }

    free(temp);
    free(nameBuffer);

    return 0;
}

