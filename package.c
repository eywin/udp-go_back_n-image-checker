#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "package.h"
#include "readfiles.h"

// brukes for å gi pakken et serienummer
// den starter på 0 og teller oppover
unsigned char TL_serial_no = 0;


void free_TL_pck(struct TL_pck *p)
{
    if (p->data) free(p->data);
    free(p);
}


struct TL_pck *make_payload_package(char* buffer, int buffer_len, unsigned char last_ACK)
{
    struct TL_pck *p = malloc(sizeof(struct TL_pck));

    p->len = (sizeof(struct TL_pck) + buffer_len - sizeof(char *));
    p->s_no = TL_serial_no++;
    p->ACK = last_ACK;
    p->flag = 0x1;
    p->not_used = 0x7f;
    p->data = buffer;

    return p;
}


struct TL_pck *make_ACK_package(unsigned char s_no)
{
    struct TL_pck *p = malloc(sizeof(struct TL_pck));

    p->len = (sizeof(struct TL_pck) - sizeof(char *));
    p->s_no = s_no;
    p->ACK = 0;
    p->flag = 0x2;
    p->not_used = 0x7f;
    p->data = NULL;

    return p;
}


struct TL_pck *make_terminating_package(unsigned char last_ACK)
{
    struct TL_pck *p = malloc(sizeof(struct TL_pck));

    p->len = (sizeof(struct TL_pck) - sizeof(char *));
    p->s_no = TL_serial_no++;
    p->ACK = last_ACK;
    p->flag = 0x4;
    p->not_used = 0x7f;
    p->data = NULL;

    return p;
}


// tar inn en package-struct og gjør den om til et sendbart buffer
char *make_TL_pck_buffer(struct TL_pck *p)
{
    char *buffer = malloc(p->len);

    int i = 0;
    int val = htons(p->len);

    memcpy(buffer + i, (char*)&val, sizeof(int));
    i += sizeof(int);

    memcpy(buffer + i, &p->s_no, sizeof(char));
    i += sizeof(char);

    memcpy(buffer + i, &p->ACK, sizeof(char));
    i += sizeof(char);

    memcpy(buffer + i, &p->flag, sizeof(char));
    i += sizeof(char);

    memcpy(buffer + i, &p->not_used, sizeof(char));
    i += sizeof(char);

    if (p->data != NULL)
    {
        int payload_len = p->len - (sizeof(int) + (sizeof(char) * 4));
        memcpy(buffer + i, p->data, sizeof(char) * payload_len);
    }

    return buffer;

}


struct TL_pck *unpack_TL_pck(char* src)
{
    struct TL_pck *dest = malloc(sizeof(struct TL_pck));

    int i = 0;
    dest->len = ntohs(*(int*)(src + i));
    i += sizeof(int);

    dest->s_no = *(src + i);
    i += sizeof(char);

    dest->ACK = *(src + i);
    i += sizeof(char);

    dest->flag = *(src + i);
    i += sizeof(char);

    dest->not_used = *(src + i);
    i += sizeof(char);

    if (dest->len > 8)
    {
        dest->data = malloc(sizeof(char) * dest->len-i);
        memcpy(dest->data, src + i, sizeof(char) * dest->len-i);
    }
    else
    {
        dest->data = NULL;
    }

    return dest;
}
