#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <libgen.h>
#include <arpa/inet.h>
#include <dirent.h>

#include "pgmread.h"
#include "readfiles.h"

int AL_serial_no = 0; // brukes for å identifisere hver AL_pck


void print_AL_pck_info(struct AL_pck *p)
{
    printf("(%d) [%d B] %s\n", p->number, p->img_len, p->filename);
}


void free_filelist(struct filelist *l)
{
    for (int n = 0; n < l->count; n++)
        free(l->list[n]);
    free(l->list);
    free(l);
}


void free_AL_pck(struct AL_pck *p)
{
    free(p->filename);
    free(p->img);
    free(p);
}


void free_img_list(struct AL_pck **l, int count)
{
    for (int i = 0; i < count; i++)
        free_AL_pck(l[i]);
    free(l);
}


// privat hjelpefunksjon
char *read_file(char *filename, int *bytes_read)
{
    struct stat filestat;
    if (stat(filename, &filestat) == -1)
    {
        fprintf(stderr, "file: %s\n", filename);
        perror("error reading file!");
        exit(EXIT_FAILURE);
    }

    FILE *f = fopen(filename, "r");
    if (!f)
    {
        perror("error opening file");
        exit(EXIT_FAILURE);
    }

    char *buffer = malloc(sizeof(char) * filestat.st_size);

    *bytes_read = fread(buffer, sizeof(char), filestat.st_size, f);
    if (!*bytes_read)
    {
        perror("error reading file");
        exit(EXIT_FAILURE);
    }
    fclose(f);
    return buffer;
}


// private hjelpefunksjon
char *get_basename(char *src)
{
    /* Implementere dette like godt selv, da jeg fikk så
       mange valgrind-feil ved å bruke strcpy() og basename() */
    int src_len = strlen(src);
    int b_ptr = 0;
    for (int i = 0; i < src_len; i++)
    {
        if (src[i] == '/')
        {
            if (b_ptr < i) b_ptr = i+1;
        }
    }
    int b_len = src_len - b_ptr;

    char* basename = malloc(sizeof(char) * b_len+1);
    for (int i = b_ptr; i < src_len; i++)
    {
        basename[i-b_ptr] = src[i];
    }
    basename[b_len] = '\0';
    return basename;
}


struct AL_pck *read_image(char *filename)
{
    struct AL_pck *pck = malloc(sizeof(struct AL_pck));
    pck->number = AL_serial_no++;

    pck->filename = get_basename(filename);
    pck->filename_len = strlen(pck->filename)+1;

    pck->img = read_file(filename, &pck->img_len);
    return pck;
}


struct AL_pck **read_images(struct filelist *fl)
{
    struct AL_pck **img_list;
    img_list = malloc(sizeof(struct AL_pck *) * fl->count);
    for (int i = 0; i < fl->count; i++)
    {
        img_list[i] = read_image(fl->list[i]);
    }
    return img_list;
}


// privat hjelpefunksjon
struct filelist *parse_filenames(char *buffer, int buffer_len)
{
    struct filelist *names = malloc(sizeof(struct filelist));
    names->count = 0;

    // går igjennom og teller newlines
    for (int i = 0; i < buffer_len; i++)
        if ((char)buffer[i] == '\n')
            names->count++;

    // mallocer plass til pekerlisten over navn (en for hver linje)
    names->list = malloc( sizeof(char*) * names->count );

    // leser ut tokens
    char *w = strtok(buffer, " \t\n");
    names->list[0] = malloc(sizeof(char) * strlen(w)+1);
    strcpy(names->list[0], w);

    int n = 1;
    while (n<names->count)
    {
        w = strtok(NULL, " \t\n");
        names->list[n] = malloc(sizeof(char) * strlen(w)+1);
        strcpy(names->list[n], w);
        n++;
    }
    return names;
}


struct filelist *read_filenames(char* filename)
{
    int l;
    char *buff = read_file(filename, &l);
    struct filelist *names = parse_filenames(buff, l);
    free(buff);
    return names;
}


char *make_AL_pck_buffer(struct AL_pck *img, int *buffer_len)
{
    *buffer_len = ((sizeof(int) * 2) +
                   (sizeof(char) * img->filename_len) +
                   (sizeof(char) * img->img_len));
    char *buffer = malloc(*buffer_len * sizeof(char));

    int i = 0;
    int val = htons(img->number);

    memcpy(buffer + i, (char*)&val, sizeof(int));

    i += sizeof(int);
    val = htons(img->filename_len);
    memcpy(buffer + i, (char*)&val, sizeof(int));


    i += sizeof(int);
    memcpy(buffer + i, img->filename, sizeof(char) * img->filename_len);

    i += sizeof(char) * img->filename_len;
    memcpy(buffer + i, img->img, sizeof(char) * img->img_len);

    return buffer;
}


struct AL_pck *unpack_AL_pck(char* buffer, int buffer_len)
{
    struct AL_pck *img = malloc(sizeof(struct AL_pck));
    int i = 0;

    img->number = ntohs(*(int*)(buffer));

    i += sizeof(int);
    img->filename_len = ntohs(*(int*)(buffer + i));

    i += sizeof(int);
    img->filename = malloc(sizeof(char) * img->filename_len);
    memcpy(img->filename, buffer + i, sizeof(char) * img->filename_len);

    i += sizeof(char)*img->filename_len;
    img->img_len = buffer_len - i;
    img->img = malloc(sizeof(char) * img->img_len);
    memcpy(img->img, buffer + i, sizeof(char) * img->img_len);

    return img;
}



struct AL_pck **read_directory(const char *dir, int *count)
{
    DIR *d = opendir(dir);
    if (d == NULL)
    {
        fprintf(stderr, "directory: %s\n", dir);
        perror("error opendir");
        exit(EXIT_FAILURE);
    }
    struct dirent *rdir;
    int dir_len = strlen(dir);
    int imgs = 0;
    // teller først antall filer i mappen for malloce plass til bildene-pakkene
    while ( (rdir = readdir(d)) != NULL )
    {
        if (rdir->d_type == 8) imgs++;
    }
    closedir(d);

    struct AL_pck **img_list;
    img_list = malloc(sizeof(struct AL_pck *) * imgs);
    d = opendir(dir);
    int ind = 0;
    while ( (rdir = readdir(d)) != NULL )
    {
        if (rdir->d_type == 8)
        {
            int filename_len = strlen(rdir->d_name);
            // legger på directory under filnavnet, slik at vi kan lese bildet
            // med read_image()
            int full_len = sizeof(char)*dir_len + sizeof(char)*filename_len +2;

            char full_filename[full_len];
            for(int i = 0; i < full_len ;i++)
            {
                if (i < dir_len) full_filename[i] = dir[i];
                else if (i == dir_len) full_filename[i] = '/';
                else if (i == full_len) full_filename[i] = '\0';
                else full_filename[i] = rdir->d_name[(i-dir_len-1)];
            }
            img_list[ind] = read_image(full_filename);
            ind++;
        }
    }
    closedir(d);
    *count = imgs;
    return img_list;
}
