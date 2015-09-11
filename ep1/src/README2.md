First Come First Served

Foi implementado em src/algorithms.c:fcfs_run .

Em linhas gerais, aguarda uma CPU ficar livre (via semaforo g_available_processors), depois disso
pega o primeiro processo da fila de processos prontos (ready_processes).

Shortest Job First

Foi implementado em src/algorithms.c:jsf_run .

Em linhas gerais, aguarda uma CPU ficar livre (via semaforo g_available_processors), depois olha pra lista de processos prontos (ready_processes) e escolhe o que possui menor dt.

Shortest Remaining Time Next

Foi implementado em src/algorithms.c:srtn_run,srtn_on_ready .

Em linhas gerais, aguarda um novo processo (ou um grupo deles) ficar pronto (via src/algorithms.c:srtn_on_ready), 

Round Robin

Escalonamento por prioridade

Escalonamento em tempo real com deadlines rigidos
