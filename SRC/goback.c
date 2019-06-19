
#include <stdio.h>
#include <stdlib.h> /* para malloc, free, srand, rand */
#include <string.h>

/*******************************************************************
 BIT ALTERNANTE E GO-BACK-N EMULADOR: VERSÃO 1.1  J.F.Kurose

   Propriedades da rede:
   - rede com atraso médio de cinco unidades de tempo (maior se 
     há outras mensagens no canal: GBN), mas pode ser maior
   - pacotes podem ser corrompidos (no cabeçalho ou dados)
     ou perdidos, de acordo com uma probabilidade definida pelo usuário
   - pacotes são entregues na ordem que enviados
     (embora alguns podem ser perdidos).
**********************************************************************/

#define BIDIRECTIONAL 0 /* mude para 1 se quiser tornar a rede bidirecional */
                        /* e escreva a rotina B_output */
#define BUFSIZE 64 /* Tamanho do buffer do envio e recebimendo do pacote */

/* uma "msg" é uma unidade de dados passada da aplicação(camada 5, código feito) para*/
/* camada 4 (código a ser feito).  Ela contém os dados (caracteres) a serem entregues */
/* para a outra camada 5 via protocolo de transporte a ser implementado*/
struct msg
{
    char data[20];
};

/* um pacote é uma unidade de dados passado da camada 4 (código a ser implementado) para camada */
/* 3 (já implementada).  A struct pacote deve ser utilizada está abaixo*/
struct pkt
{
    int seqnum;
    int acknum;
    int checksum;
    char payload[20];
};


void tolayer3(int AorB, struct pkt packet);
void tolayer5(int AorB, char datasent[20]);
void starttimer(int AorB, float increment);
void stoptimer(int AorB);


/********* PARA O TRABALHO PRECISA ESCREVER AS PRÓXIMAS 6 ROTINAS *********/

struct emissor
{
    int sequencia;
    int prox_sequencia;
    int swnd;
    float rtt_estimado;
    int prox_buffer;
    struct pkt pacote_buffer[BUFSIZE];
} A;

struct receptor
{
    int espera_sequencia;
    struct pkt pacote_envio;
} B;

int calculaChecksum(struct pkt *p)
{
    int sum = 0;
    sum += p->seqnum + p->acknum;
    /* Percorre payload do pacote */
    for (int i = 0; i < 20; ++i)
        sum += p->payload[i];
    return sum;
}


/* Chamada na camada 5, passa o dado a ser enviado ao host B*/
void A_output(struct msg message)
{
    if (A.prox_buffer - A.sequencia >= BUFSIZE){
        printf("A_output: Buffer não suporta pacote | %s\n", message.data);
        return;
    }
    printf("|-------------------------------------------------------------|\n");
    printf("|A_output: %d, Coloca no buffer o pacote  [%s]|\n", A.prox_buffer, message.data);
    struct pkt *packet = &A.pacote_buffer[A.prox_buffer % BUFSIZE];
    packet->seqnum = A.prox_buffer;
    memcpy(packet->payload, message.data, 20);
    packet->checksum = calculaChecksum(packet);
    ++A.prox_buffer;

    while (A.prox_sequencia < A.prox_buffer && A.prox_sequencia < A.sequencia + A.swnd){
        struct pkt *packet = &A.pacote_buffer[A.prox_sequencia % BUFSIZE];
        printf("|Envia Janela: %d, Envia pacote [%s]          |\n", packet->seqnum, packet->payload);
        printf("|-------------------------------------------------------------|\n\n");
        tolayer3(0, *packet);
        if (A.sequencia == A.prox_sequencia)
            starttimer(0, A.rtt_estimado);
        ++A.prox_sequencia;
    }
}

void B_output(struct msg message)
{
}

/* Chamada da camada 3, quando pacotes chegama para camada 4 */
void A_input(struct pkt packet)
{
    if (packet.acknum < A.sequencia){
        printf("|A_input: Recebeu ack duplicado [%d]                                 |\n", packet.acknum);
        return;
    }
    if (packet.checksum != calculaChecksum(&packet)){
        printf("A_input: Pacote recebido corrompido\n");
        return;
    }
    printf("|A_input: Recebeu ack [%d]                                          |\n", packet.acknum);
    A.sequencia = packet.acknum + 1;
    if (A.sequencia == A.prox_sequencia){
        stoptimer(0);
        printf("|A_input: Para cronometro do timeout                                |\n");

        while (A.prox_sequencia < A.prox_buffer && A.prox_sequencia < A.sequencia + A.swnd){
            struct pkt *packet = &A.pacote_buffer[A.prox_sequencia % BUFSIZE];
            printf("Envia Janela(swnd): %d | Envia pacote | %s\n", packet->seqnum, packet->payload);
            tolayer3(0, *packet);
            if (A.sequencia == A.prox_sequencia)
                starttimer(0, A.rtt_estimado);
            ++A.prox_sequencia;
        }
    } else {
        starttimer(0, A.rtt_estimado);
        printf("|A_input: Rtt, %f                               |\n\n", A.rtt_estimado);
    }
}

/* chamado quando temporizador de A estoura */
void A_timerinterrupt(void)
{
    for (int i = A.sequencia; i < A.prox_sequencia; ++i){
        struct pkt *packet = &A.pacote_buffer[i % BUFSIZE];
        printf("|------------------------------------------------------------------|\n");
        printf("|A_timerinterrupt: %d, Envia pacote novamente [%s] |\n", packet->seqnum, packet->payload);
        tolayer3(0, *packet);
    }
    starttimer(0, A.rtt_estimado);
    printf("|A_timerinterrupt: Rtt, %f                                  |\n", A.rtt_estimado);
}

/* Será chamada uma única vez antes de qualquer outra rotina de A seja chamada */
/* Você pode utilizar para qualquer inicialização */
void A_init()
{
    A.sequencia = 1; 
    A.swnd = 8;
    A.prox_sequencia = 1;
    A.rtt_estimado = 15;
    A.prox_buffer = 1;
}

/* Como a transferência é simplex de A-para-B, não há B_output() */

/* chamada da camada 3, quando um pacote chega para camada 4 em B*/
void B_input(struct pkt packet)
{
    if (packet.seqnum != B.espera_sequencia){
        printf("|B_input: %d, Sequencia errada envia ack                           |\n", B.pacote_envio.acknum);
        tolayer3(1, B.pacote_envio);
        return;
    }
    if (packet.checksum != calculaChecksum(&packet)){
        printf("B_input: %d | Pacote corrompido envia ack\n", B.pacote_envio.acknum);
        tolayer3(1, B.pacote_envio);
        return;
    }
    printf("|--------------------------------------------------|\n");
    printf("|B_input: %d, Recebeu pacote [%s]  |\n", packet.seqnum, packet.payload);
    tolayer5(1, packet.payload);

    printf("|B_input: %d, Envia ack                             |\n", B.espera_sequencia);
    printf("|--------------------------------------------------|\n\n");
    B.pacote_envio.acknum = B.espera_sequencia;
    B.pacote_envio.checksum = calculaChecksum(&B.pacote_envio);
    tolayer3(1, B.pacote_envio);

    ++B.espera_sequencia;
}

/* chamada quando temporizador de B estoura */
void B_timerinterrupt()
{
}

/* Será chamada uma única vez antes de qualquer outra rotina de B seja chamada */
/* Você pode utilizar para qualquer inicialização */
void B_init()
{
    B.espera_sequencia = 1;
    B.pacote_envio.seqnum = -1;
    B.pacote_envio.acknum = 0;
    memset(B.pacote_envio.payload, 0, 20);
    B.pacote_envio.checksum = calculaChecksum(&B.pacote_envio);
}

/*****************************************************************
***************** EMULADOR DE REDE INICIA ABAIXO ***********
o código emula da camada 3 p/ baixo:
  - emula a transmissão e entrega, com possibilidade de corrupção de bits
    e perda de pacote, de pacotes através das interfaces 3/4
  - manipula o início/parada de um temporizador, e gera interrupção deles, o que resulta
    na chamada aos manipuladores de tempo que serão implementados
  - gera mensagem a ser enviada, passada da camada 5 p/ 4

NÃO HÁ NECESSIDADE DE LER OU ENTENDER O CÓDIGO ABAIXO PARA RESOLVER O TRABALHO. 
VOCÊ NÃO DEVE TOCAR OU REFERENCIAR QUALQUER STRUCT ABAIXO. 
Se tiver interessado em entender, tudo bem. Mas, não modifique!
******************************************************************/

struct event
{
    float evtime;       /* evento tempo */
    int evtype;         /* código do tipo de evento */
    int eventity;       /* entidade onde ocorre o evento */
    struct pkt *pktptr; /* ptr p/ pacote associado, se houver, com evento */
    struct event *prev;
    struct event *next;
};
struct event *evlist = NULL; /* a lista de evento */

/* Definição de protótipos de funções a serem implementadas */
/* mais abaixo, no código provido.*/
void init();
void generate_next_arrival();
void insertevent(struct event *p);

/* eventos possíveis: */
#define TIMER_INTERRUPT 0
#define FROM_LAYER5 1
#define FROM_LAYER3 2

#define OFF 0
#define ON 1
#define A 0
#define B 1

int TRACE = 1;   /* p/ debbug */
int nsim = 0;    /* número de msgs da camada 5 p/ 4 */
int nsimmax = 0; /* número de msgs a gerar, então para */
float time = (float)0.000;
float lossprob;    /* probabilidade de um pacote ser descartado  */
float corruptprob; /* probabilidade de um bit no pacote ser trocado */
float lambda;      /* taxa de chegada de msgs da camada 5 */
int ntolayer3;     /* número enviado p/ camada 3 */
int nlost;         /* número perdido em média */
int ncorrupt;      /* número corrompido em média */

void init();
void generate_next_arrival(void);
void insertevent(struct event *p);

void main()
{
    struct event *eventptr;
    struct msg msg2give;
    struct pkt pkt2give;

    int i, j;
    /* char c; // variável local não referenciada, removida */

    init();
    A_init();
    B_init();

    while (1)
    {
        // puts("//////////////AQUI WHILE////////////////");
        eventptr = evlist; /* pega prócimo evento p/ simular */
        if (eventptr == NULL)
            goto terminate;
        evlist = evlist->next; /* remove o evento da lista */
        if (evlist != NULL)
            evlist->prev = NULL;
        if (TRACE >= 2)
        {
            printf("\nEVENT tempo: %f,", eventptr->evtime);
            printf("  tipo: %d", eventptr->evtype);
            if (eventptr->evtype == 0)
                printf(", parada de temporizador  ");
            else if (eventptr->evtype == 1)
                printf(", da camada 5 ");
            else
                printf(", da camada 3 ");
            printf(" entidade: %d\n", eventptr->eventity);
        }
        time = eventptr->evtime; /* atualiza tempo p/ próximo evento */
        if (eventptr->evtype == FROM_LAYER5)
        {
            if (nsim < nsimmax)
            {
                if (nsim + 1 < nsimmax)
                    generate_next_arrival(); /* define a chegada futura */
                /* preenche msg com string de mesmo caractere */
                j = nsim % 26;
                for (i = 0; i < 20; i++)
                    msg2give.data[i] = 97 + j;
                msg2give.data[19] = 0;
                if (TRACE > 2)
                {
                    printf("          MAINLOOP: dado entregue ao estudante: ");
                    for (i = 0; i < 20; i++)
                        printf("%c", msg2give.data[i]);
                    printf("\n");
                }
                nsim++;
                if (eventptr->eventity == A)
                    A_output(msg2give);
                else
                    B_output(msg2give);
            }
        }
        else if (eventptr->evtype == FROM_LAYER3)
        {
            pkt2give.seqnum = eventptr->pktptr->seqnum;
            pkt2give.acknum = eventptr->pktptr->acknum;
            pkt2give.checksum = eventptr->pktptr->checksum;
            for (i = 0; i < 20; i++)
                pkt2give.payload[i] = eventptr->pktptr->payload[i];
            if (eventptr->eventity == A) /* entrega o pacote */
                A_input(pkt2give);       /* chamando entidade apropriada */
            else
                B_input(pkt2give);
            free(eventptr->pktptr); /* libera a memória do pacote */
        }
        else if (eventptr->evtype == TIMER_INTERRUPT)
        {
            if (eventptr->eventity == A)
                A_timerinterrupt();
            else
                B_timerinterrupt();
        }
        else
        {
            printf("ERRO INTERNO: tipo de envento desconhecido \n");
        }
        free(eventptr);
    }

terminate:
    printf("///////////////////terminate/////////////////");
    printf(" Simulador terminou em tempo %f\n depois de enviar %d msgs da camada 5\n", time, nsim);
}

void init() /* inicializa simulador */
{
    int i;
    float sum, avg;
    float jimsrand();

    printf("-----  Simulador Para e Espera - Version 1.1 -------- \n\n");
   printf("|---------------------------------------------------------------------------|\n");
   printf("|Informe a quantidade de mensagem a simular:                                | = ");
   scanf(" %d",&nsimmax);
   printf("|Informe a probabilidade de perda de pacote [informe 0.0 sem perdas]:       | = ");
   scanf(" %f",&lossprob);
   printf("|Informe a probabilidade de corromper pacote [informe 0.0 sem corromper]:   | = ");
   scanf(" %f",&corruptprob);
   printf("|Informe o tempo entre msgs do emissor na camada 5[ > 0.0]:                 | = ");
   scanf(" %f",&lambda);
   printf("|Informe TRACE ou Nível de detalhamento:                                    | = ");// >2 p/ mensagens de debug
   scanf(" %d",&TRACE);
   printf("|---------------------------------------------------------------------------|\n\n");


    srand(9999);      /* inicia gerador de números aleatórios */
    sum = (float)0.0; /* testa números aleatórios gerados para o código */
    for (i = 0; i < 1000; i++)
        sum = sum + jimsrand(); /* jimsrand() deve ser uniforme em [0,1] */
    avg = sum / (float)1000.0;
    if (avg < 0.25 || avg > 0.75)
    {
        printf("Provavel que a geração de números aleatórios na sua máquina\n");
        printf("seja diferente que a esperada pelo emulador. Por favor olhe\n");
        printf("a rotina jimsrand() no código do emulador. Desculpe. \n");
        exit(0);
    }

    ntolayer3 = 0;
    nlost = 0;
    ncorrupt = 0;

    time = (float)0.0;       /* inicia tempo em 0.0 */
    generate_next_arrival(); /* inicia lista de eventos */
}

/****************************************************************************/
/* jimsrand(): retorna float entre [0,1].  Rotina abaixo é utilizada */
/* para isolar toda geração aleatória de número em um lugar. Assumimos que*/
/* a função rand() retorna um int entre [0,mmm]        */
/****************************************************************************/
float jimsrand()
{
    double mmm = RAND_MAX;     /* maior int  - DEPENDENTE DE MÁQUINA!!!!!!!!   */
    float x;                   /* alguns estudantes talvez precisem modificar mmm*/
    x = (float)(rand() / mmm); /* x deve ser uniforme entre [0,1] */
    return (x);
}

/********************* ROTINAS DE MANIPULAÇÃO DE EVENTOS *******/
/*  O pŕoximo conjunto de rotinas manipula a lista de eventos  */
/*****************************************************/

void generate_next_arrival()
{
    double x, log(), ceil();
    struct event *evptr;
    /* char *malloc(); // redefinição de malloc removida */
    /* float ttime; // variável local não referenciada removida*/
    /* int tempint; // variável local não referenciada removida*/

    if (TRACE > 2)
        printf("          GERA PRÓXIMA CHEGADA: cria nova chegada\n");

    x = lambda * jimsrand() * 2; /* x é uniforme entre [0,2*lambda] */
                                 /* tendo média de lambda        */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtime = (float)(time + x);
    evptr->evtype = FROM_LAYER5;
    if (BIDIRECTIONAL && (jimsrand() > 0.5))
        evptr->eventity = B;
    else
        evptr->eventity = A;
    insertevent(evptr);
}

void insertevent(p) struct event *p;
{
    struct event *q, *qold;

    if (TRACE > 2)
    {
        printf("            INSERE EVENTO: tempo é %lf\n", time);
        printf("            INSERE EVENTO: tempo futuro será %lf\n", p->evtime);
    }
    q = evlist; /* q aponta p/ cabeça da lista em que struct p é inserida */
    if (q == NULL)
    { /* lista vazia */
        evlist = p;
        p->next = NULL;
        p->prev = NULL;
    }
    else
    {
        for (qold = q; q != NULL && p->evtime > q->evtime; q = q->next)
            qold = q;
        if (q == NULL)
        { /* fim da lista */
            qold->next = p;
            p->prev = qold;
            p->next = NULL;
        }
        else if (q == evlist)
        { /* cabeça da lista */
            p->next = evlist;
            p->prev = NULL;
            p->next->prev = p;
            evlist = p;
        }
        else
        { /* meio da lista */
            p->next = q;
            p->prev = q->prev;
            q->prev->next = p;
            q->prev = p;
        }
    }
}

void printevlist()
{
    struct event *q;
    /* int i; // variável local não referenciada removida*/
    printf("--------------\nLista de evento segue:\n");
    for (q = evlist; q != NULL; q = q->next)
    {
        printf("Tempo evento: %f, tipo: %d entidade: %d\n", q->evtime, q->evtype, q->eventity);
    }
    printf("--------------\n");
}

/********************** ROTINAS QUE PODEM SER CHAMADAS ***********************/

/* chamada p/ cancelar um temporizador já iniciado */
void stoptimer(AorB) int AorB; /* A or B está tentando parar o temporizador */
{
    struct event *q; /* ,*qold; // variável local não referenciada removida*/

    if (TRACE > 2)
        printf("          PARA TEMPORIZADOR: parando temporizador em %f\n", time);
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
    for (q = evlist; q != NULL; q = q->next)
        if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB))
        {
            /* remove este evento */
            if (q->next == NULL && q->prev == NULL)
                evlist = NULL;        /* remove o primeiro e único evento da lista*/
            else if (q->next == NULL) /* fim da lista - há um na frente */
                q->prev->next = NULL;
            else if (q == evlist)
            { /* início da lista - deve haver evento depois*/
                q->next->prev = NULL;
                evlist = q->next;
            }
            else
            { /* meio da lista */
                q->next->prev = q->prev;
                q->prev->next = q->next;
            }
            free(q);
            return;
        }
    printf("Warning: incapaz de cancelar seu temporizador. Não estava rodando.\n");
}

void starttimer(AorB, increment) int AorB; /* A ou B está tentando parar o temporizador */
float increment;
{

    struct event *q;
    struct event *evptr;
    /* char *malloc(); // redefinição de malloc removida*/

    if (TRACE > 2)
        printf("          INICIA TEMPORIZADOR: inicia temporizador em %f\n", time);
    /* verifica se o temporizador já iniciou, se sim, então avisa */
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
    for (q = evlist; q != NULL; q = q->next)
        if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB))
        {
            printf("|Warning: tentativa de iniciar temporizador já iniciado|\n");
            return;
        }

    /* cria um evento futuro p/ quando temporizador estourar*/
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtime = (float)(time + increment);
    evptr->evtype = TIMER_INTERRUPT;
    evptr->eventity = AorB;
    insertevent(evptr);
}

/************************** PARA CAMADA 3 ***************/
void tolayer3(AorB, packet) int AorB; /* A ou B está tentando parar o temporizador */
struct pkt packet;
{
    // puts("///////////AQUI CAMADA 3 (REDE)");
    struct pkt *mypktptr;
    struct event *evptr, *q;
    /* char *malloc(); // redefinição de malloc removida */
    float lastime, x, jimsrand();
    int i;

    ntolayer3++;

    /* simula perda: */
    if (jimsrand() < lossprob)
    {
        nlost++;
        if (TRACE > 0)
            printf("          PARA CAMADA 3: pacote sendo perdido\n");
        return;
    }

    /* faz uma cópia do pacote para que o código continue com ela caso*/
    /* código implementado descarte*/
    mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
    mypktptr->seqnum = packet.seqnum;
    mypktptr->acknum = packet.acknum;
    mypktptr->checksum = packet.checksum;
    for (i = 0; i < 20; i++)
        mypktptr->payload[i] = packet.payload[i];
    if (TRACE > 2)
    {
        printf("          PARA CAMADA 3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
               mypktptr->acknum, mypktptr->checksum);
        for (i = 0; i < 20; i++)
            printf("%c", mypktptr->payload[i]);
        printf("\n");
    }

    /* cria evento futuro para chegada de pacote no outro lado */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtype = FROM_LAYER3;      /* pacote será retirado da camada 3 */
    evptr->eventity = (AorB + 1) % 2; /* evento ocorre na outra entidade */
    evptr->pktptr = mypktptr;         /* salva ponteiro para cópia do pacote */
                                      /* finalmente, computa o tempo de chegada do pacote no outro host.
   o meio não reordena, garante que o pacote chega entre 1 e 10
   unidades de tempo após a última chegada de pacote*/
    lastime = time;
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
    for (q = evlist; q != NULL; q = q->next)
        if ((q->evtype == FROM_LAYER3 && q->eventity == evptr->eventity))
            lastime = q->evtime;
    evptr->evtime = lastime + 1 + 9 * jimsrand();

    /* simula corrupção: */
    if (jimsrand() < corruptprob)
    {
        ncorrupt++;
        if ((x = jimsrand()) < .75)
            mypktptr->payload[0] = 'Z'; /* corrompe payload */
        else if (x < .875)
            mypktptr->seqnum = 999999;
        else
            mypktptr->acknum = 999999;
        if (TRACE > 0)
            printf("          PARA CAMADA 3: pacote sendo corrompido\n");
    }

    if (TRACE > 2)
        printf("          PARA CAMADA 3: escalonando chegada no outro lado\n");
    insertevent(evptr);
}

void tolayer5(AorB, datasent) int AorB;
char datasent[20];
{
    // puts("///////////AQUI CAMADA 5 (APP)");
    int i;
    if (TRACE > 2)
    {
        printf("          PARA CAMADA 5: dado recebido: ");
        for (i = 0; i < 20; i++)
            printf("%c", datasent[i]);
        printf("\n");
    }
}