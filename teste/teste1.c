#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/t2fs.h"

#define MAXOPEN 20

void printHelp(int action);
void printAllHelp();
void printOpenFiles();
void print_dir(DIRENT2 entry);
void printDirTree(char* name, int level);

int getFreeIndex();
int getChosenAction(char* action);

int indexes[MAXOPEN];
char* names[MAXOPEN];

enum {VIEWOPEN, CREATE, DELETE, OPEN, CLOSE, READ, WRITE, SEEK, CREATEDIR, DELETEDIR, CHDIR, LISTDIR, TREE, EXIT} chosenAction;

int main() {

    int i;
    for (i = 0; i < MAXOPEN; i++) {
        indexes[i] = -1;
        names[i] = malloc(sizeof(char)*255);
    }

    char currentDir[BUFSIZ];
    do {
        getcwd2(currentDir, BUFSIZ);
        printf("\n%s>", currentDir);
        gets(currentDir);

        chosenAction = getChosenAction(currentDir);

        switch (chosenAction) {
        case VIEWOPEN:
            printOpenFiles();
            break;
        case CREATE: {
            char* filename = strtok(NULL," ");

            if (filename == NULL) {
                printHelp(CREATE);
                break;
            }

            FILE2 handle = create2(filename);
            if (handle == -1)
                printf(" Nao foi possivel criar o arquivo");
            else {
                int index = getFreeIndex();

                indexes[index] = handle;
                getcwd2(names[index], 255);
                strcat(names[index], "/");
                strcat(names[index], filename);
                printf(" Arquivo criado e aberto com handle %d", handle);
            }
            break;
        }
        case DELETE: {
            char* filename = strtok(NULL," ");

            if (filename == NULL) {
                printHelp(DELETE);
                break;
            }

            FILE2 handle = delete2(filename);
            if (handle == -1)
                printf(" Nao foi possivel deletar o arquivo");
            else
                printf(" Arquivo deletado com sucesso");
            break;
        }
        case OPEN: {
            char* filename = strtok(NULL," ");

            if (filename == NULL) {
                printHelp(OPEN);
                break;
            }

            FILE2 handle = open2(filename);
            if (handle == -1)
                printf(" Nao foi possivel abrir o arquivo");
            else {
                int index = getFreeIndex();

                indexes[index] = handle;
                getcwd2(names[index], 255);
                strcat(names[index], "/");
                strcat(names[index], filename);
                printf(" Arquivo aberto com handle %d", handle);
            }
            break;
        }
        case READ: {
            char* handleChar = strtok(NULL," ");
            char* bytesChar = strtok(NULL," ");

            if (handleChar == NULL || bytesChar == NULL) {
                printHelp(READ);
                break;
            }

            int bytes = atoi(bytesChar), handle = atoi(handleChar);
            char buffer[bytes];
            int read = read2(handle, buffer, bytes);
            if (read == -1)
                printf(" Nao foi possivel ler o arquivo especificado");
            else if (read == 0)
                printf(" Fim do arquivo ja atingido");
            else {
                printf(" Foram lidos %d bytes do arquivo:\n", read);
                int i;
                for (i = 0; i < read; i++)
                    printf("%c", buffer[i]);
            }
            break;
        }
        case WRITE: {
            char* handleChar = strtok(NULL," ");
            char* token = strtok(NULL," ");

            if (handleChar == NULL || token == NULL) {
                printHelp(WRITE);
                break;
            }

            char* textToWrite = malloc(sizeof(char)*BUFSIZ);
            while (token != NULL) {
                strcat(textToWrite, token);
                token = strtok(NULL, " ");
                if (token != NULL)
                    strcat(textToWrite, " ");
            }

            int handle = atoi(handleChar);
            int written = write2(handle, textToWrite, strlen(textToWrite));

            if (written == -1)
                printf(" Nao foi possivel escrever no arquivo especificado");
            else
                printf(" Foram escritos %d bytes no arquivo\n", written);

            break;
        }
        case CLOSE: {
            char* handleChar = strtok(NULL," ");

             if (handleChar == NULL) {
                printHelp(CLOSE);
                break;
            }
            int handle = atoi(handleChar);
            int ret = close2(handle);
            if (ret == 0) {
                int i;
                for (i = 0; i < MAXOPEN; i++)
                    if (indexes[i] == handle)
                        indexes[i] = -1;
                printf("Fechei o arquivo com sucesso");
            } else
                printf("Nao foi possivel fechar o arquivo");
            break;
        }
        case SEEK: {
            char* handleChar = strtok(NULL," ");
            char* bytesChar = strtok(NULL," ");

            if (handleChar == NULL || bytesChar == NULL) {
                printHelp(SEEK);
                break;
            }

            int bytes = atoi(bytesChar), handle = atoi(handleChar);
            int read = seek2(handle, bytes);
            if (read == -1)
                printf(" Nao foi possivel posicionar o ponteiro na posicao indicada");
            else
                printf(" Ponteiro posicionado corretamente");

            break;
        }
        case CREATEDIR: {
            char* filename = strtok(NULL," ");

            if (filename == NULL) {
                printHelp(CREATEDIR);
                break;
            }

            FILE2 handle = mkdir2(filename);
            if (handle == -1)
                printf(" Nao foi possivel criar o diretorio");
            else
                printf(" Diretorio criado com sucesso");
            break;
        }
        case DELETEDIR: {
            char* filename = strtok(NULL," ");

            if (filename == NULL) {
                printHelp(DELETEDIR);
                break;
            }

            FILE2 handle = rmdir2(filename);
            if (handle == -1)
                printf(" Nao foi possivel deletar o diretorio");
            else
                printf(" Diretorio deletado com sucesso");
            break;
        }
        case CHDIR: {
            char* dirName = strtok(NULL," ");

             if (dirName == NULL) {
                printHelp(CHDIR);
                break;
            }

            int ret = chdir2(dirName);
            if (ret == -1)
                printf("Nao foi possivel mudar o diretorio");
            break;
        }
        case LISTDIR: {
            DIR2 handle = opendir2(".");
            DIRENT2* file = malloc(sizeof(DIRENT2));
            while (readdir2(handle, file) == 0)
                print_dir(*file);
            printf("\n");
            closedir2(handle);
            free(file);
            break;
        }
        case TREE: {
            printDirTree(".", 0);
            break;
        }
        case EXIT:
            break;
        default:
            printAllHelp();
            break;
        }

    } while (chosenAction != EXIT);

    return 0;
}

int getFreeIndex() {
    int i;
    for (i = 0; i < MAXOPEN; i++)
        if (indexes[i] == -1)
            return i;
    return -1;
}

void printDirTree(char* name, int level) {
    DIR2 handle = opendir2(name);
    DIRENT2* file = malloc(sizeof(DIRENT2));

    int i;
    if (level == 0) {
        for (i = 0; i < level; i++)
            printf("|   ");
        printf("%s\n", name);
    }

    while (readdir2(handle, file) == 0) {
        if (strcmp(file->name, ".") == 0 || strcmp(file->name, "..") == 0)
            continue;
        for (i = 0; i < level; i++)
            printf("|   ");
        printf("|-- %s\n", file->name);
        if (file->fileType == TYPEVAL_DIRETORIO) {
            char* newName = malloc(sizeof(char)*255);
            strcpy(newName, name);
            strcat(newName, "/");
            strcat(newName, file->name);
            printDirTree(newName, level+1);
            free(newName);
        }
    }
    closedir2(handle);
    free(file);
}

void printOpenFiles() {

    printf("\n    \tArquivos abertos");
    printf("\n Handle\tNome");
    int i;
    for (i = 0; i < MAXOPEN; i++)
        if (indexes[i] != -1)
            printf("\n %5d\t%s", indexes[i], names[i]);
}

void printHelp(int action) {

    switch (action) {
    case CREATE:
        printf("create [filename]\n");
        printf("\n Cria o arquivo com o nome especificado.");
        printf("\n Caso a criacao funcione, sera informado a handle do arquivo aberto.");
        printf("\n O arquivo podera ser escrito, lido ou fechado usando a handle informada.");
        printf("\n A criacao ira falhar caso o diretorio esteja cheio (14 entradas).");
        break;
    case DELETE:
        printf("rm [filename]\n");
        printf("\n Deleta o arquivo com o nome especificado");
    case OPEN:
        printf("open [filename]\n");
        printf("\n Abre o arquivo com o nome especificado.");
        printf("\n Caso a abertura funcione, sera informado a handle do arquivo aberto, e a mesma podera");
        printf(" ser consultada com a funcao \"viewopen\"");
        printf("\n O arquivo podera ser escrito, lido ou fechado usando a handle informada.");
        printf("\n Aberturas consecutivas de um mesmo arquivo retornarao handles diferentes.");
        break;
    case READ:
        printf("read [handle] [bytes]\n");
        printf("\n Le os proximos [bytes] bytes do arquivo, imprimindo-os como texto.");
        break;
    case WRITE:
        printf("write [handle] [text]\n");
        printf("\n Escreve o texto [text] no arquivo especificado por [handle].");
        printf("\n Por questao de simplicidade de implementacao, so e possivel inserir uma linha de cada vez.");
        printf("\n Para inserir multiplas linhas de texto, e necessario chamar o comando multiplas vezes.");
        break;
    case CLOSE:
        printf("close [handle]\n");
        printf("\n Fecha o arquivo informado pelo [handle]. O tamanho do arquivo sera cortado para ser");
        printf(" igual ao ponteiro atual do arquivo (modificado por seek, read e write).");
        break;
    case SEEK:
        printf("seek [handle] [offset]\n");
        printf("\n Altera a posicao do ponteiro do arquivo indicado por [handle].");
        printf("\n Caso seja indicado um offset maior do que o tamanho do arquivo, retorna um erro.");
        printf("\n Para posicionar o ponteiro no final do arquivo, offset pode ser -1.");
        break;
    case CREATEDIR:
        printf("mkdir [dirname]\n");
        printf("\n Cria o diretorio com o nome especificado.");
        printf("\n A criacao ira falhar caso o diretorio pai esteja cheio (14 entradas).");
        break;
    case DELETEDIR:
        printf("rmdir [dirname]\n");
        printf("\n Deleta o diretorio com o nome especificado.");
        printf("\n A delecao ira falhar caso o diretorio tenha qualquer entrada nele (excluindo . e ..)");
        break;
    case CHDIR:
        printf("cd [dirname]\n");
        printf("\n Altera o diretorio corrente para o indicado por [dirname].");
        break;
    case LISTDIR:
        printf("ls\n");
        printf("\n Imprime todos os arquivos e diretorios filhos do diretorio atual.");
    case TREE:
        printf("tree\n");
        printf("\n Imprime todos os arquivos e diretorios filhos do diretorio atual, recursivamente");
        break;
    }
}

void printAllHelp() {

    printf("Acoes disponiveis:");
    printf("\n viewopen");
    printf("\n create [filename]");
    printf("\n rm [filename]");
    printf("\n open [filename]");
    printf("\n read [handle] [bytes]");
    printf("\n write [handle] [text]");
    printf("\n close [handle]");
    printf("\n seek [handle] [offset]");
    printf("\n mkdir [dirname]");
    printf("\n rmdir [dirname]");
    printf("\n cd [dirname]");
    printf("\n ls");
    printf("\n tree");
    printf("\n exit");

    printf("\n\n Todos os caminhos (filename e dirname) podem ser absolutos ou relativos.");
    printf("\n Para caminhos absolutos, comecar o caminho com \"/\". Todos os outros caminhos sao considerados relativos.");
}

int getChosenAction(char* text) {

    char* action = strtok(text, " ");

    if (strncmp(action, "viewopen", 8) == 0)
        return VIEWOPEN;
    else if (strncmp(action, "create", 5) == 0)
        return CREATE;
    else if (strncmp(action, "rm", 2) == 0)
        return DELETE;
    else if (strncmp(action, "open", 4) == 0)
        return OPEN;
    else if (strncmp(action, "read", 4) == 0)
        return READ;
    else if (strncmp(action, "write", 5) == 0)
        return WRITE;
    else if (strncmp(action, "close", 5) == 0)
        return CLOSE;
    else if (strncmp(action, "seek", 4) == 0)
        return SEEK;
    else if (strncmp(action, "mkdir", 5) == 0)
        return CREATEDIR;
    else if (strncmp(action, "rmdir", 5) == 0)
        return DELETEDIR;
    else if (strncmp(action, "cd", 2) == 0)
        return CHDIR;
    else if (strncmp(action, "ls", 2) == 0)
        return LISTDIR;
    else if (strncmp(action, "tree", 4) == 0)
        return TREE;
    else if (strncmp(action, "exit", 4) == 0)
        return EXIT;
    else
        return -1;
}

void print_dir(DIRENT2 entry) {
    if (entry.fileType == TYPEVAL_DIRETORIO)
        printf("\n<DIR>\t\t%s", entry.name);
    else
        printf("\n<FILE>\t%d\t%s", entry.fileSize, entry.name);
}

