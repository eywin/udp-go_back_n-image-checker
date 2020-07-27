#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include "readfiles.h"
#include "package.h"
#include "send_packet.h"
#include "LList.h"

#define ACK_SIZE 8
#define WINDOW_SIZE 7

#define SECS_TO_TIMEOUT 5
#define TIMEVAL_TIMEOUT_SECS 2
#define TIMEVAL_TIMEOUT_USECS 550000

static int client_socket = 0;


void quit_program()
{
    if (client_socket) close(client_socket);
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


void check_loss_range(float ploss)
{
    // påser at vi får en gyldig prosent
    if (ploss < 0 || ploss > 1)
    {
        fprintf(stderr, "Packet loss percentage is out of legal range, must be between 0 and 100\n");
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


void check_launch_args(int argc, char const *argv[])
{
    // påset at vi har fått riktig antall argumenter, skriver ut forklarende
    // tekst dersom det er feil
    if (argc != 5)
    {
        fprintf(stderr, "Unable to launch! Wrong number of arguments.\n"
                "start the program with: ./client <server ip> <server port> "
                "<filename(list)> <packet loss percentage>\n");
        quit_program();
    }

    if (!isdigit(*argv[2]) || !isdigit(*argv[4]))
    {
        fprintf(stderr,
            "[Error] Invalid launch arguments,"
            "can't parse port/percentage numbers.\n");
        quit_program();
    }
}


int check_ACK(char *buf, unsigned char s_no)
{
    // Sjekker om ACKen vi får er den vi forventer
    struct TL_pck *p = unpack_TL_pck(buf);
    if (p->flag ==  0x2) // is ACK
    {
        if (p->s_no == s_no)
        {
            free_TL_pck(p);
            return 1;
        }
    }
    free_TL_pck(p);
    return 0;
}


struct sockaddr_in setup_connection(const char *ip, int port, socklen_t *addrlen)
{
    struct sockaddr_in server_addr;
    *addrlen = sizeof(server_addr);

    client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    check_error(client_socket, "socket");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    return server_addr;
}


int send_message(char* message, int mlen, struct sockaddr_in server_addr, socklen_t addrlen)
{
    int sentbytes = send_packet(client_socket, message, mlen, 0,
                                (struct sockaddr *)&server_addr, addrlen);
    check_error(sentbytes, "send");
}


char *prepare_next_package(int img_nr, struct AL_pck **AL_pcks, unsigned char last_ACK)
{
    int AL_buffer_len;
    /* Konverterer til senbare buffer */
    char *AL_buffer = make_AL_pck_buffer(AL_pcks[img_nr], &AL_buffer_len);
    struct TL_pck *TL_pck = make_payload_package(AL_buffer, AL_buffer_len, last_ACK);
    char *TL_buffer = make_TL_pck_buffer(TL_pck);

    /* frigjør minnet som ikke er i bruk */
    free(AL_buffer);
    free(TL_pck);
    return TL_buffer;
}


void start_client(const char *filename, const char *ip, int connection_port)
{
    /* Open socket */
    socklen_t addrlen;
    struct sockaddr_in server_addr = setup_connection(ip, connection_port, &addrlen);

    /* Leser filene inn i minne */

    /* argv[3] blir aldri endret inne i funksjonene, den blir kun lest
    for å åpne filen som inneolder filnavnet til bildene, så det er trygt
    å caste den til (char *) */
    struct filelist *filelist = read_filenames((char*)filename);
    int pcks_to_send = filelist->count;
    struct AL_pck **AL_pcks = read_images(filelist);

    // opretter sendevinduet
    struct LList *window = make_LL();
    char *buf;
    char ACK_buffer[ACK_SIZE];
    int sentbytes, buf_len, rc, img_nr = 0, last_ACK = -1;
    time_t last_ACK_time, now;

    struct timeval timeout;
    timeout.tv_sec = TIMEVAL_TIMEOUT_SECS;
    timeout.tv_usec = TIMEVAL_TIMEOUT_USECS;

    // legget til socket til select-watch-listen
    fd_set fd_watch;
    FD_ZERO(&fd_watch);
    FD_SET(client_socket, &fd_watch);

    last_ACK_time = time(NULL);

    while (last_ACK < pcks_to_send-1)
    {
        /* Fyller winduet og sender pakkene */
        while( window->elements < WINDOW_SIZE && img_nr < pcks_to_send)
        {
            printf("Sending package %d\n", img_nr);
            buf = prepare_next_package(img_nr, AL_pcks, last_ACK);
            buf_len = ntohs(*(int*)buf);
            send_message(buf, buf_len, server_addr, addrlen);
            // Legger til pakken i winduet over sendte, ikke kvitterte pakker
            LL_add(window, buf);
            img_nr++; // for å holde styr på hvilke bilder som er sendt
        }


        /* Venter på ACK */
        rc = select(FD_SETSIZE, &fd_watch, 0, 0, &timeout);
        check_error(rc, "select");

        /* sjekk om select har fått napp!
           dette er da om vi har fått en ACK fra server
           ACK må være for første element i window (den eldste pakken
           som ikke er ACK'et) */
        if(FD_ISSET(client_socket, &fd_watch))
        {
            ACK_buffer[0] = '\0';
            rc = read(client_socket, ACK_buffer, ACK_SIZE);
            check_error(rc, "read ACK");

            // sjekker om det er riktig ACK vi har fått
            if (check_ACK(ACK_buffer, last_ACK+1))
            {
                last_ACK_time = time(NULL); // reset timeren for ACK
                free( LL_pop(window) ); // fjerner fra window
                last_ACK++;
                printf("ACK for expected package %d received\n", (int)last_ACK);
            }
            // ACK for feil pakke blir ignorert
        }
        else
        {
            printf("select timeout, no packages\n");
        }
        FD_SET(client_socket, &fd_watch);

        now = time(NULL);
        // resend window (go-back-n)
        if ((now - last_ACK_time) >= SECS_TO_TIMEOUT )
        {
            printf("Timeout! resending window\n");
            for (int i = 0; i < window->elements; i++)
            {
                buf = LL_get(window, i);
                buf_len = ntohs(*(int*)buf);
                printf("sending package %d\n", i+last_ACK+1);
                send_message(buf, buf_len, server_addr, addrlen);
            }
            last_ACK_time = time(NULL);
        }
        // reset timeout struct
        timeout.tv_sec = TIMEVAL_TIMEOUT_SECS;
        timeout.tv_usec = TIMEVAL_TIMEOUT_USECS;
        printf("Currently %d packages 'in flight', waiting for ACK %d \n", window->elements, last_ACK+1);
    }

    /* Ferdig med å sende alle bildene. Avslutter med å frigjøre
       minne som ikke skal brukes mer */
    free_LL(window);
    free_img_list(AL_pcks, filelist->count);
    free_filelist(filelist);

    // sender så en termineringspakke til serveren
    struct TL_pck *term = make_terminating_package(0);
    char *termbuff = make_TL_pck_buffer(term);
    sentbytes = send_message(termbuff, 8, server_addr, addrlen);
    printf("Terminating package sent\n");
    free_TL_pck(term);
    free(termbuff);
}


int main(int argc, char const *argv[])
{
    /* Sjekker argumenter */
    check_launch_args(argc, argv);

    int connection_port = atoi(argv[2]);
    check_port_range(connection_port);

    float packet_loss_percentage = (float)atoi(argv[4])/100;
    check_loss_range(packet_loss_percentage);
    set_loss_probability(packet_loss_percentage);

    // printer ut litt info om kjøringen
    printf("Server IP: %s\n", argv[1]);
    printf("Port: %d\n", connection_port);
    printf("Filename: %s\n", argv[3]);
    printf("Packet loss percentage: %f\n", packet_loss_percentage);

    // inneholder hovedløkka
    start_client(argv[3], argv[1], connection_port);

    // til slutt lukker vi socket-fildestriptoren og avslutter
    close(client_socket);

    return EXIT_SUCCESS;
}
