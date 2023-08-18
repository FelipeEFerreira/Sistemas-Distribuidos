/*
            Disciplina: Sistemas Distribuídos 2023 - 1
            Curso: Engenharia de Computação - UFGD - FACET
            Trabalho 1
            Implementação de programação paralela para controle de aeroportos
            Docente: Rodrigo Porfírio da Silva Sacchi
            
            Discentes:
            Felipe Emanuel Ferreira     RGA: 20200712175441
            Rafael Alcalde Sanabria     RGA: 20180712144101
*/

//inclusão de bibliotecas
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "mpi.h"

//structs usadas
struct Voo{
    int voo_id = -1;
    int origem;
    int destino;
    int chegada_partida;
    int tempo_voo = 0;
};

struct Aeroporto{
    int id;
    int lc = 0;
    int numPousos = 0;
    int numDecolagens = 0;
    std::vector<Voo> pousos;
    std::vector<Voo> decolagens;
};


//protótipos das funções
void menu(); //imprime menu
void maxLc(const int m_lc, int* lc); //Compara Relógio lógico e soma +1
void trataConflito(Aeroporto* aero); //trata os conflitos
void tabela(const Aeroporto* aero); //imprime tabela dos aeroportos



int main(int argc, char* argv[])
{
    int size, rank;
    int opt = -1;
    Voo voo;
    int origem, destino, tempo_voo, m_lc;
    MPI_Status status;
    MPI_Request request = MPI_REQUEST_NULL;


    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    Aeroporto aero = {rank};

    while(opt != 3)
    {
        destino = -1;

        if(rank == 0){
            menu();
            scanf("%d", &opt);
            if(opt == 1 || opt == 2)
            {
            	do
            	{
                    printf("Qual o aeroporto origem? (0 a %d)\n", (size-1));
                    scanf("%d", &origem);
                } while(origem >= size || origem < 0);
            }
        }

        MPI_Bcast(&opt, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&origem, 1, MPI_INT, 0, MPI_COMM_WORLD);

        switch(opt)
        {
            case 1: //Cadastra voos

            //Lê as informações do Voo
            //É feito no processo de rank 0, pois é o unico com acesso ao buffer de leitura
                if(rank == 0)
                {
                    do
                    {
                        printf("Destino:\n");
                        scanf("%d", &destino);
                    }while(destino == origem || destino < 0 || destino >= size);
                    do
                    {
                        printf("Tempo previsto de voo:\n");
                        scanf("%d", &tempo_voo);
                    }while(tempo_voo <= 0);

                    voo = {-1, origem, destino, -1, tempo_voo};

                    //Passendo as informações para o aeroporto origem
                    MPI_Isend(&voo, sizeof(Voo), MPI_BYTE, origem, 0, MPI_COMM_WORLD, &request);
                }

                //Informa os processos de qual será o destino
                MPI_Bcast(&destino, 1, MPI_INT, 0, MPI_COMM_WORLD);

		//tratamento das informações pela origem
                if(rank == origem)
                {
                    MPI_Recv(&voo, sizeof(Voo), MPI_BYTE, 0, 0, MPI_COMM_WORLD, &status);
                    aero.numDecolagens++;
                    voo.chegada_partida = aero.lc;
                    aero.lc++; //incrementa Relógio Lógico
                    m_lc = aero.lc;
                    voo.voo_id = rank*10 + aero.numDecolagens;
                    aero.decolagens.push_back(voo);
                    MPI_Send(&m_lc, 1, MPI_INT, destino, 0, MPI_COMM_WORLD);
                    MPI_Send(&voo, sizeof(Voo), MPI_BYTE, destino, 1, MPI_COMM_WORLD);
                }
                
                //Tratamento das informações pelo destino
                if(rank == destino)
                {
                    MPI_Recv(&m_lc, 1, MPI_INT, origem, 0, MPI_COMM_WORLD, &status);
                    maxLc(m_lc, &aero.lc); //compara relógios logicos
                    MPI_Recv(&voo, sizeof(Voo),MPI_INT, origem, 1, MPI_COMM_WORLD, &status);
                    
                    voo.chegada_partida += voo.tempo_voo;

                    aero.numPousos++;
                    aero.pousos.push_back(voo);

                    trataConflito(&aero);
                    
                }
		break;
            case 2: //imprime tabela
                if(rank == origem)
                    tabela(&aero);
                MPI_Barrier(MPI_COMM_WORLD);
                break;
            case 3: //sai do programa
		if(rank == 0)
                    printf("Saindo...\n");
                break;
            default: //bad input
                if(rank == 0)
                    printf("opção invalida\n");
        }
    }
    MPI_Finalize();
    return 0;
}


void maxLc(const int m_lc, int* lc)
{
    if(m_lc > *lc)
        *lc = m_lc;
    *lc += 1;
}

void trataConflito(Aeroporto* aero)
{
    //tratando conflito de pouso e decolagens
    int aux = aero->numPousos - 1;
    for(int i = 0; i < aero->numDecolagens; i++)
    {
        if(aux == i)
            break;
        if(aero->decolagens[i].chegada_partida == aero->pousos[aux].chegada_partida)
        {
            aero->decolagens[i].chegada_partida++;
            printf("Voo de id %d adiado devido a um pouso!\n", aero->decolagens[i].voo_id);
            aux = i;

            //checando possiveis conflito de decolagens
            for(int j = 0; j < aero->numDecolagens; j++)
            {
                if(aux == j)
                    break;
                if(aero->decolagens[j].chegada_partida == aero->decolagens[aux].chegada_partida)
                {
                    if(aero->decolagens[aux].tempo_voo > aero->decolagens[j].tempo_voo)
                    {
                        aero->decolagens[j].chegada_partida++;
                        printf("Voo de id %d adiado...\n", aero->decolagens[j].voo_id);
                        aux = j;
                    }
                    else
                    {
                        aero->decolagens[aux].chegada_partida++;
                        printf("Voo de id %d adiado...\n", aero->decolagens[aux].voo_id);
                    }
                    j = 0;
                }
            }
        }
    }

    //tratando conflito de pousos
    aux = aero->numPousos - 1;
    for(int i = 0; i < aero->numPousos; i++)
    {
        if(i == aux)
            break;
        if(aero->pousos[i].chegada_partida == aero->pousos[aux].chegada_partida)
        {
            printf("Conflito de pousos...\n");
            if(aero->pousos[aux].tempo_voo > aero->pousos[i].tempo_voo)
            {
                aero->pousos[i].chegada_partida++;
                aero->pousos[i].tempo_voo++;
                printf("Voo de id %d terá que aguardar para pousar...\n", aero->pousos[i].voo_id);
                aux = i;
            }
            else
            {
                aero->pousos[aux].chegada_partida++;
                aero->pousos[aux].tempo_voo++;
                printf("Voo de id %d terá que aguardar para pousar...\n", aero->pousos[aux].voo_id);
            }
            i = 0;
        }
    }
}

void menu()
{
    printf("Selecione uma opcao\n");
    printf("1 - Informar dados do voo\n");
    printf("2 - Consultar decolagens/pousos agendados para hoje\n");
    printf("3 - Sair\n");
}

void tabela(const Aeroporto* aero){
    printf("\n---------------------------------------------------------\n");
    printf("| Codigo:\t|\t\t%d\t\t\t|\n", aero->id);
    printf("---------------------------------------------------------\n");
    printf("| Pousos:\t| %d\t\t| Decolagens:\t| %d\t|\n", aero->numPousos, aero->numDecolagens);
    printf("---------------------------------------------------------\n");
    printf("| Pousos\t| Origem\t| Chegada\t| Tempo\t|\n");
    printf("---------------------------------------------------------\n");
    for(int i = 0; i < (int)aero->pousos.size(); i++){
        printf("| %02d\t\t| %d\t\t| %d\t\t| %d\t|\n", aero->pousos[i].voo_id, aero->pousos[i].origem, aero->pousos[i].chegada_partida, aero->pousos[i].tempo_voo);
    }
    printf("---------------------------------------------------------\n");
    printf("| Decolagens\t| Destino\t| Partida\t| Tempo\t|\n");
    for(int i = 0; i < (int)aero->decolagens.size(); i++){
        printf("| %02d\t\t| %d\t\t| %d\t\t| %d\t|\n", aero->decolagens[i].voo_id, aero->decolagens[i].destino, aero->decolagens[i].chegada_partida, aero->decolagens[i].tempo_voo);
    }
    printf("---------------------------------------------------------\n");
}
