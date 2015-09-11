# Simulador

O simulador usa as estruturas ProcessTable e ProcessControl block para tomar decisoes.

t0 eh o instante em segundos que o processo chega ao sistema.

GetReadyProcesses() -> processos cujo estado eh READY

Estando no instante t de tempo se temos 3 processos cujo t0 == t, o algoritmo deve escolher
um desses candidatos.

O arquivo nos da a historia: passado, presente, futuro.
Desse modo o simulador deve, para um dado instante do tempo tomar decisoes.

BEGIN history
time++
if time == t0
	consider processes -> enqueue -> update Process table
O que quero dizer com isso eh que o Simulador faz o seguinte:
