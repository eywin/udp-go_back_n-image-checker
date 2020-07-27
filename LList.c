#include <stdio.h>
#include <stdlib.h>

#include "LList.h"

struct LList *make_LL()
{
    struct LList *list = malloc(sizeof(struct LList));
    list->elements = 0;
    list->head = NULL;
    return list;

}

char *LL_get(struct LList *list, int i)
{
    if (i > list->elements)
    {
        fprintf(stderr, "not that many elements in list\n");
        return NULL;
    }


    struct node *n = list->head;
    if (n == NULL)
    {
        fprintf(stderr, "HEAD==NULL, EMPTY LIST\n");
        return NULL;
    }

    for (int j=0; j < i; j++)
    {
        n = n->next;
    }
    return n->pck;
}


void free_LL(struct LList *list)
{
    if (list->head == NULL)
    {
        free(list);
        return;
    }
    struct node *temp, *n = list->head;
    while (n->next != NULL)
    {
        temp = n;
        n = n->next;
        if(temp->pck) free(temp->pck);
        free(temp);
    }
    free(n);
    free(list);
}

void LL_add(struct LList *list, char *p)
{
    struct node *n, *tmp;
    n = malloc(sizeof(struct node));
    n->pck = p;
    n->next = NULL;

    if (list->head == NULL)
    {
        list->head = n;
    }
    else
    {
        tmp = list->head;
        while (tmp->next != NULL)
        {
            tmp = tmp->next;
        }
        tmp->next = n;
    }
    list->elements++;
}

char *LL_pop(struct LList *list)
{
    struct node *n;
    char *data;
    n = list->head;

    if (n == NULL)
    {
        fprintf(stderr, "HEAD==NULL, EMPTY LIST\n");
        return NULL;
    }
    data = n->pck;

    if (n->next == NULL)
        list->head = NULL;

    list->head = n->next;
    free(n);

    list->elements--;
    return data;
}
