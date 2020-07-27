#ifndef LLIST
#define LLIST


// implementert som en FIFO liste

struct LList {
    int elements;
    struct node *head;
};

struct node
{
    char* pck;
    struct node *next;
};


/* Oppretter en lenkeliste-struct */
struct LList *make_LL();

/* Henter ut char* fra noden med indeks i, indeksene starter på 0 og går til
   n-1, der n er antall elementer i lenkelisten.
   Dataen blir ikke fjernet, men man får pekeren til dataen, så om man free'er
   den får man en "tom node" */
char *LL_get(struct LList *list, int i);

/* Frijør minne for alt i Lenkelisten */
void free_LL(struct LList *list);

/* Legger til et element bakerst i lenkelisten (FIFO)*/
void LL_add(struct LList *list, char *p);

/* Fjerner det første elementet, returnerer pekeren til dataen */
char *LL_pop(struct LList *list);


#endif
