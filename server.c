#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "package.h"
#include "readfiles.h"
#include "pgmread.h"
#include "send_packet.h"

#define ACK_SIZE 8
#define PCK_BUF_SIZE 1542 // max size for ethernet pck


unsigned char expected_serial_no = 0;
int server_socket = 0;


void quit_program()
{
    if (server_socket) close(server_socket);
    exit(EXIT_FAILURE);
}

void check_error(int ret, char *desc)
{
    if (ret == -1)
    {
        perror(desc);
        quit_program();
    }
}


int send_message(char* message, int mlen, struct sockaddr_in client_addr, socklen_t addrlen)
{
    int sentbytes;
    if ( (sentbytes = send_packet(server_socket, message, mlen, 0,
        (struct sockaddr *)&client_addr, addrlen)) == -1)
        {
            perror("error can't send message");
            exit(EXIT_FAILURE);
        }
}


void compare_img_to_dir(struct AL_pck **dir_imgs, int img_count, struct AL_pck *recv, FILE *output_file)
{
    /* sammenlikner bildene i structene dir_imgs(disse bildene er lest inn fra
       mappen som blir oppgitt nå man starter programmet) med bildet mottatt fra
       klienten. Den skriver så ut til outputfilen om den finner en match */
    struct Image *recvd_img = Image_create(recv->img);
    struct Image *img_in_dir;
    char *whitespace = " \n";
    char *unknown = "UNKNOWN\n";
    char *temp;
    int found = 0;
    fwrite(recv->filename, sizeof(char), recv->filename_len-1, output_file);
    fwrite(whitespace, sizeof(char), 1, output_file);

    for (int i = 0; i < img_count; i++)
    {
        /* Av en eller annen grunn endrer/ødelegger pgmread bilde-bufferene,
           så jeg er nødt til å sende inn en kopi */
        temp = malloc(sizeof(char)*dir_imgs[i]->img_len);
        memcpy(temp, dir_imgs[i]->img, sizeof(char)*dir_imgs[i]->img_len);
        img_in_dir = Image_create(temp);
        if (Image_compare(img_in_dir, recvd_img))
        {
            found = 1;
            fwrite(dir_imgs[i]->filename, sizeof(char), dir_imgs[i]->filename_len-1, output_file);
            fwrite(whitespace+1, sizeof(char), 1, output_file);
            free(temp);
            Image_free(img_in_dir);
            /* skriver kun ut første treff ! (så om det finnes duplikater,
               vil den ikke finne disse) */
            break;
        }
        free(temp);
        Image_free(img_in_dir);
    }
    if (!found) fwrite(unknown, sizeof(char), strlen(unknown), output_file);
    Image_free(recvd_img);
}


void send_ACK(unsigned char s_no, struct sockaddr_in addr, socklen_t addrlen)
{
    /* Lager og sender ACKer etter spesifiser serienummer */
    printf("Sending ACK for package %d\n", (int)s_no );
    struct TL_pck *ACK_pck = make_ACK_package(s_no);
    char *ACK_buff = make_TL_pck_buffer(ACK_pck);

    int sentbytes = send_message(ACK_buff, ACK_SIZE, addr, addrlen);
    check_error(sentbytes, "send_ack");

    free_TL_pck(ACK_pck);
    free(ACK_buff);
}

void check_launch_args(int argc, char const *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Unable to launch! Wrong number of arguments.\n"
                "start the program with: ./server <listening port> "
                "<directory name> <output file>\n");
        quit_program();
    }
}


void check_port_range(int port)
{
    // enkel fuksjon for å sjekke at man oppgir en gyldig port
    if (port < 1024 || port > 65535)
    {
        fprintf(stderr, "Port is out of legal range, must be between 1025 and 65535\n");
        quit_program();
    }
}


int main(int argc, char const *argv[])
{
    // sjekker argumentene
    check_launch_args(argc, argv);

    set_loss_probability(0);
    int connection_port = atoi(argv[1]);
    check_port_range(connection_port);

    // åpner output filen
    FILE *output_file = fopen(argv[3], "w+");
    // henter alle bilder i mappen inn i minnet
    int dir_img_count;
    struct AL_pck **dir_imgs = read_directory(argv[2], &dir_img_count);

    printf("Listening port: %d\n", connection_port);


    // Setter opp socket så vi kan motta meldinger
    struct sockaddr_in server_addr;
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    check_error(server_socket, "error can't open socket");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(connection_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // gjør porten gjenbrukbar
    int optval = 0;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
    // binder porten
    int ret = bind(server_socket, (struct sockaddr*)&server_addr,sizeof(server_addr));
    check_error(ret, "bind failed");

    // ferdig med å sette opp, starter å lytte etter pakker
    struct sockaddr_in source;
    socklen_t source_len = sizeof(source);

    int recvbytes, sentbytes, pck_recvd = 0;
    struct TL_pck *TL_unpacked;
    struct AL_pck *AL_unpacked;
    char *recv_buffer, *ACK_buff;

    int terminated = 0;

    while (!terminated) // denne skal kjøre til vi mottar en termineringspakke
    {
        recv_buffer = malloc(sizeof(char) * PCK_BUF_SIZE);
        printf("Waiting for client...\n");
        recvbytes = recvfrom(server_socket, recv_buffer, PCK_BUF_SIZE, 0,
                                 (struct sockaddr *) &source, &source_len);

        // leser pakken inn i en struct
        TL_unpacked = unpack_TL_pck(recv_buffer);
        free(recv_buffer);

        printf("Package %d received [%d B]\n", TL_unpacked->s_no,recvbytes);

        // ser på serienummeret
        if (TL_unpacked->s_no != expected_serial_no)
        {
            // feil pakke
            printf("Expected package %d\n", expected_serial_no);
            if (TL_unpacked->s_no < expected_serial_no)
            {
                send_ACK(TL_unpacked->s_no, source, source_len);
                /* sender ACK på nytt når serienummert er lavere enn det man
                venter på. Det kan hende ACK pakkene ikke har kommet frem til
                klient */
            }
            free_TL_pck(TL_unpacked);
        }
        else
        {
            // riktig pakke
            expected_serial_no++;
            if (TL_unpacked->flag == 0x1)
            {
                printf("Got expected package, comparing images\n");
                AL_unpacked = unpack_AL_pck(TL_unpacked->data, TL_unpacked->len - 8);
                compare_img_to_dir(dir_imgs, dir_img_count, AL_unpacked, output_file);

                send_ACK(TL_unpacked->s_no, source, source_len);
                free_AL_pck(AL_unpacked);
                free_TL_pck(TL_unpacked);

                pck_recvd++;
            }

            else if (TL_unpacked->flag == 0x4)
            {
                printf("Termination package received, closing program.\n\n");
                free_TL_pck(TL_unpacked);
                terminated = 1;
            }
        }
    }

    // frigjør resterende minne
    for (int i = 0; i < dir_img_count; i++)
    {
        free_AL_pck(dir_imgs[i]);
    }
    free(dir_imgs);
    // lukker output filen
    fclose(output_file);
    // lukker forbinnelsen
    close(server_socket);

    return EXIT_SUCCESS;
}
