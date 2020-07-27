#ifndef PACKAGE
#define PACKAGE

// transportlagspakken sin struct
struct TL_pck
{
    int len;
    unsigned char s_no;
    unsigned char ACK;
    unsigned char flag;
        /*
         * 0x1: 1 if contains data
         * 0x2: 1 if contains ACK
         * 0x4: 1 if terminating pck
         */
     unsigned char not_used; // Må være 0x7f
     char *data;
};

// frigjør minne i pakken
void free_TL_pck(struct TL_pck *p);

// tar imot et applikasjonslags(pakke)-buffer og lager en struct, som igjen kan
// gjøre som til et sendbart buffer
struct TL_pck *make_payload_package(char* buffer, int buffer_len, unsigned char last_ACK);

// lager enkelt og greit en kvitteringspakke
struct TL_pck *make_ACK_package(unsigned char last_ACK);

// lager en termineringspakke
struct TL_pck *make_terminating_package(unsigned char last_ACK);

// gjør om en TL-pakke-struct til et sendbart buffer
char *make_TL_pck_buffer(struct TL_pck *p);

// Denne pakker ut bufferet og returnerer en TL-pakke-struct
struct TL_pck *unpack_TL_pck(char* src);


#endif /* PACKAGE */
