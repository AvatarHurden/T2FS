
#ifndef __BITMAP2__
#define	__BITMAP2__

/**
	Retorna a informação do estado do bloco lógico “blockNumber” do disco.
	Retorna “0”, se o bloco estiver ocupado e um valor diferente de zero, caso esteja livre.
*/
extern	int isBlockFree2 (unsigned int blockNumber);

/**
	Libera o bloco lógico “blockNumber”. Isso é feito escrevendo na área de bitmap do disco.
	Retorna “0”, se a escrita foi bem sucedida; retorna um valor diferente de zero, caso tenha ocorrido algum erro.
*/
extern	int freeBlock2 (unsigned int blockNumber);

/**
	Aloca (ocupa) o bloco lógico “blockNumber”. Isso é feito escrevendo na área de bitmap do disco.
	Retorna “0”, se a escrita foi bem sucedida; retorna um valor diferente de zero, caso tenha ocorrido algum erro.
*/
extern	int allocBlock2 (unsigned int blockNumber);

/**
	Procura no bitmap de alocação de blocos por um bloco livre.
	Retorna o número do bloco encontrado; retorna “0” se não encontrou nenhum bloco livre ou em caso de erro.
*/
extern	unsigned int searchFreeBlock2 (void);

#endif
