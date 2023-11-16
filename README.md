# Trabalho-SO
Trabalho académico desenvolvido no âmbito da cadeira de Sistemas Operativos do curso de Engenharia Informática (2022-2023).

## 🎯Objetivo/Descrição
O	 trabalho desenvolvido simula	um	ambiente	simplificado	de	 Internet	das	Coisas	onde	vários	 sensores	 enviam	 informação para	 um	 ponto	 centralizado, que	 por	 sua	 vez	armazena	estes	dados,	gera	estatísticas	e	despoleta	alertas	quando	certas	condições	são	atingidas.	

![imagem geral do trabalho](https://github.com/Simao-Correia-Santos/Trabalho-SO/assets/138619513/4c34cfb2-15f1-4933-ab97-1c900cd80889)


Ao iniciar o sistema, tanto os utilizadores como os sensores podem mandar dados através de dois named pipes, dedicados para cada um, onde serão recebidos por duas
threads, uma para os sensores outra para os users, essas threads colocam os dados na internal queue caso esta não se encontre cheia e enviam um sinal ao dispatcher, caso esta se encontre cheia os dados dos sensores são eliminados e os pedidos dos users ficarão há espera na thread numa variável de condição por um slot livre. Quando existir um slot livre esta recebe um sinal enviado pelo dispatcher. O dispatcher por outro lado fica há espera numa variável de condição por um nó na internal queue se esta se encontrar vazia. Este remove sempre o primeiro nó, pois os dados dos sensores são appended à internal queue enquanto os pedidos dos utilizadores são inseridos no início da internal queue devido à sua prioridade. Tanto a remoção como a inserção de nós na internal queue são controladas por um semáforo.

O dispatcher de seguida verifica o valor de um semáforo que indica se há algum worker disponível e caso exista procura o id do worker e envia a informação pelo
unnamed pipe correspondente, o worker mete o seu estado como ocupado na memoria partilhada e efetua o pedido, caso seja um pedido de users o worker acede há memoria
partilhada e recolhe a informação (na memoria partilhada encontram-se os sensores, os alertas, as chaves e a disponibilidade dos workers, cada array tem um semáforo atribuído de modo a aumentar a eficiência do programa), caso sejam dados vindo dos sensores o worker atualiza a informação na shared memory. De seguida o worker dá post ao semáforo que sincroniza o alerts watcher e os workers, onde este se encontra há espera num sem_wait(), para verificar se os valores estão dentro dos alertas registados no sistema.

Por fim os resultados obtidos pelos pedidos dos user são devolvidos à respetiva user_console como também os alertas despoletados através de uma message queue sincronizada por um semáforo. Tanto o arranque e o fim do programa, alertas despoletados, mudança da disponibilidade dos workers, sinais recebidos e criação dos processos são escritos num ficheiro de log sincronizado com um semáforo.


## 🛠️Testar o Projeto
1. Abrir um ambiente de desenvolvimento Linux;
1. Carregar todos os ficheiros .c e Makefile para a mesma diretoria;
1. Compilar os ficheiros;
1. Abrir quantos sensores e utilizadores que quiser com ./sensor {nome} {intervalo_tempo} {chave} {min} {max} e ./user_console {id_consola}


## ✔️ Tecnologias utilizadas

- ``C``
- ``Linux``
- ``VS Code``
- ``Makefile``


