#ifndef READFILES
#define READFILES

// struct-'liste' over filnavn
struct filelist
{
    int count;
    char **list;
};


//struct for å behandle pakkene til applikasjonslaget
struct AL_pck
{
    int number;
    int filename_len;
    char *filename; // only basename
    int img_len;
    char *img;
};

// Brukt til debugging, skriver ut info om pakken
void print_AL_pck_info(struct AL_pck *p);

// Frigjør minnet i filelist-structen
void free_filelist(struct filelist *l);

// Frigjør minnet i applikasjonslagspakken
void free_AL_pck(struct AL_pck *p);

// Frigjør minnet pekt på av denne listen over alle bildepakkene
void free_img_list(struct AL_pck **l, int count);

// tar inn et bilde-filnavn og returnerer en appliasjonslagspakke
struct AL_pck *read_image(char *filename);

// denne tar imot filnavnet til tekstfilen med filnavnet til alle bildefilene
// dek kaller så på read_image() og returnerer en dobbeltpeker-liste med
// pakker til appliasjonslaget
struct AL_pck **read_images(struct filelist *fl);

// leser inn listen over bildefilene og lagrer det i en struct
// som brukes i read_images()
struct filelist *read_filenames(char* filename);

// konverterer en AL_pck-struct om til et sendbart buffer
char *make_AL_pck_buffer(struct AL_pck *img, int *buffer_len);

// konverterer tilbake et sendbart buffer om til en AL_pck struct
struct AL_pck *unpack_AL_pck(char* buffer, int buffer_len);

// leser inn alle bildene fra oppgitt mappenavn, brukes av serveren
struct AL_pck **read_directory(const char *dir, int *count);

#endif /* READFILES */
