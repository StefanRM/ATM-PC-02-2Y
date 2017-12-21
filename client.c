/*
 * Protocoale de comunicatii
 * Tema 2
 * ATM
 * client.c
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define BUFLEN 1500

int main(int argc, char *argv[])
{
    int sockfd = 0; // client socket
    int error_check; // checking erros for called functions
    char buffer[BUFLEN];
    struct sockaddr_in serv_addr; // server address

    if(argc != 3) // wrong number of parameters
    {
        printf("-10 : Eroare la apelul executabilului\n");
        exit(0);
    }

    // open client socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        printf("-10 : Eroare la apel socket() - TCP\n");
        exit(0);
    }

    // server addres information
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr.sin_addr);

    // connecting to server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("-10 : Eroare la apel connect()\n");
        exit(0);
    }

    fd_set r_set; // reading set used in select()
    fd_set tmp; // temporary used set

    // we want the reading descriptors' set (read_fds) and tmp_fds empty
    FD_ZERO(&r_set);
    FD_ZERO(&tmp);

    // adding new file descriptor (server socket) in read_fds set
    FD_SET(sockfd, &r_set);
    FD_SET(0, &r_set); // adding stdin
    
    int logged_in = 0; // if the client is logged in
    int attempt_to_log_in = 0; // if the client attempts to log in

    // UDP
    struct sockaddr_in serv_udp_addr; // server UDP address
    int sockudp = socket(PF_INET, SOCK_DGRAM, 0); // UDP socket
    if (sockudp < 0)
    {
        printf("-10 : Eroare la apel socket() - UDP\n");
        exit(0);
    }

    // UDP server addres information
    memset(&serv_udp_addr, 0, sizeof(serv_udp_addr));
    serv_udp_addr.sin_family = AF_INET;
    serv_udp_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_udp_addr.sin_addr);
    // end of UDP declare

    char last_card_nr[7]; // previous login given card number
    memset(last_card_nr, 0, 7);
    char buffer_aux[BUFLEN]; // auxiliary buffer for creating message
    char* token; // for tokenizer
    int poz; // counter for tokens
    unsigned int serv_udp_len = sizeof(serv_udp_addr); // server address length
    int attempt_to_unlock = 0; // if the client attempts to unlock card
    int len; // for string handling after fgets()

    // log file handling
    char filename[25];
    char pid[15]; // pid for filename creation
    memset(filename, 0, 25);
    memset(pid, 0, 25);
    sprintf(pid, "%d", getpid());
    strcpy(filename, "client-");
    strcat(filename, pid);
    strcat(filename, ".log");

    printf("Client's file log: %s\n", filename);

    // open file log
    int file_client = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    // end of log file --- next we have to right to file
    
    while(1)
    {
        tmp = r_set;

        // multiplex
        select(sockfd + 1, &tmp, NULL, NULL, NULL);

        // check descriptors
        if (FD_ISSET(0, &tmp)) // stdin
        {
            memset(buffer, 0 , BUFLEN);
            fgets(buffer, BUFLEN-1, stdin); // read
            len = strlen(buffer);
            if (buffer[len - 1] == '\n') // get rid of '\n'
            {
                buffer[len - 1] = '\0';
            }

            // write in file log the given command
            write(file_client, buffer, strlen(buffer));
            write(file_client, "\n", 1);


            // check client's given command
            if ((strncmp(buffer, "login", 5) == 0) && (attempt_to_unlock == 0)) // login command
            {
                memset(buffer_aux, 0, BUFLEN);
                strcpy(buffer_aux, buffer);

                // get value of card number to keep track of it
                token = strtok(buffer_aux, " ");
                poz = 0;
                while (token != NULL)
                {
                    if (poz == 1)
                    {
                        strncpy(last_card_nr, token, 6);
                    }
                    token = strtok(NULL, " ");
                    poz++;
                }

                if (logged_in == 1) // client already logged in
                {
                    printf("-2 : Sesiune deja deschisa\n");
                    // write to file log
                    write(file_client, "-2 : Sesiune deja deschisa\n", 27);
                } else {
                    // keeping in mind that clients wants to log in
                    attempt_to_log_in = 1;

                    // send message to server
                    error_check = send(sockfd,buffer,strlen(buffer), 0);
                    if (error_check < 0)
                    {
                         printf("-10 : Eroare la apel send()\n");
                     }
                }
            }
            else if ((strncmp(buffer, "logout", 6) == 0) && (attempt_to_unlock == 0)) // logout command
            {
                if (logged_in == 0) // not logged in
                {
                    printf("-1 : Clientul nu este autentificat\n");
                    // write to log file
                    write(file_client, "-1 : Clientul nu este autentificat\n", 35);
                } else {
                    logged_in = 0; // mark that client is logged out
                    
                    // send message to server
                    error_check = send(sockfd,buffer,strlen(buffer), 0);
                    if (error_check < 0)
                    {
                        printf("-10 : Eroare la apel send()\n");
                    }
                }
            }
            else if ((strncmp(buffer, "listsold", 8) == 0) && (attempt_to_unlock == 0)) // listsold command
            {
                if (logged_in == 0) // not logged in
                {
                    printf("-1 : Clientul nu este autentificat\n");
                    // write to log file
                    write(file_client, "-1 : Clientul nu este autentificat\n", 35);
                } else {
                    // send message to server
                    error_check = send(sockfd,buffer,strlen(buffer), 0);
                    if (error_check < 0)
                    {
                        printf("-10 : Eroare la apel send()\n");
                    }
                }
            }
            else if ((strncmp(buffer, "getmoney", 8) == 0) && (attempt_to_unlock == 0)) // getmoney command
            {
                if (logged_in == 0) // not logged in
                {
                    printf("-1 : Clientul nu este autentificat\n");
                    // write to log file
                    write(file_client, "-1 : Clientul nu este autentificat\n", 35);
                } else {
                    // send message to server
                    error_check = send(sockfd,buffer,strlen(buffer), 0);
                    if (error_check < 0)
                    {
                        printf("-10 : Eroare la apel send()\n");
                    }
                }
            }
            else if ((strncmp(buffer, "putmoney", 8) == 0) && (attempt_to_unlock == 0)) // putmoney command
            {
                if (logged_in == 0) // not logged in
                {
                    printf("-1 : Clientul nu este autentificat\n");
                    // write to log file
                    write(file_client, "-1 : Clientul nu este autentificat\n", 35);
                } else {
                    // send message to server
                    error_check = send(sockfd,buffer,strlen(buffer), 0);
                    if (error_check < 0)
                    {
                        printf("-10 : Eroare la apel send()\n");
                    }
                }
            }
            else if ((strncmp(buffer, "unlock", 6) == 0) && (attempt_to_unlock == 0)) // unlock command
            {
                memset(buffer, 0, BUFLEN);
                // attach last login card number to message
                strcpy(buffer, "unlock ");
                strcat(buffer, last_card_nr);

                // send message to server (UDP)
                sendto(sockudp, buffer, BUFLEN, 0, (struct sockaddr *)&serv_udp_addr, sizeof(serv_udp_addr));

                // receive message from server (UDP)
                recvfrom(sockudp, buffer, BUFLEN, 0, (struct sockaddr*)&serv_udp_addr, &serv_udp_len);

                // print message received
                printf("%s\n", buffer);

                // write to log file
                write(file_client, buffer, strlen(buffer));
                write(file_client, "\n", 1);

                // if the client can unlock the card
                if (strncmp(buffer, "UNLOCK> Trimite parola secreta", 30) == 0)
                {
                    // mark that the client wants to unlock the card
                    attempt_to_unlock = 1;
                }
            }
            else if ((strncmp(buffer, "quit", 4) == 0) && (attempt_to_unlock == 0)) // quit command
            {
                // send message to server
                error_check = send(sockfd,buffer,strlen(buffer), 0);
                if (error_check < 0)
                {
                    printf("-10 : Eroare la apel send()\n");
                }

                // close all sockets and file log
                close(sockudp);
                close(sockfd);
                close(file_client);
                break;
            } else {
                // if any other input
                // check if it is for unlocking card
                if (attempt_to_unlock == 1)
                {
                    // create message for server: last_card + secret password
                    memset(buffer_aux, 0, BUFLEN);
                    strcpy(buffer_aux, buffer);
                    memset(buffer, 0, BUFLEN);
                    strcpy(buffer, last_card_nr);
                    strcat(buffer, " ");
                    strcat(buffer, buffer_aux);

                    // send message to server (UDP)
                    sendto(sockudp, buffer, BUFLEN, 0, (struct sockaddr *)&serv_udp_addr, sizeof(serv_udp_addr));

                    // receive message from server (UDP)
                    recvfrom(sockudp, buffer, BUFLEN, 0, (struct sockaddr*)&serv_udp_addr, &serv_udp_len);

                    // print message
                    printf("%s\n", buffer);

                    // write to log file
                    write(file_client, buffer, strlen(buffer));
                    write(file_client, "\n", 1);

                    // end of unlock process
                    attempt_to_unlock = 0;
                }
            }

        } else {
            // server sent messages
            memset(buffer, 0 , BUFLEN);
            recv(sockfd, buffer, BUFLEN, 0);

            if (attempt_to_log_in == 1)
            {
                // response to login command
                // end of login process
                attempt_to_log_in = 0;
                if (strncmp(buffer, "ATM> Welcome ", 13) == 0)
                {
                    logged_in = 1; // client logged in
                }
            }

            if (strncmp(buffer, "quit", 4) == 0)
            {
                // server quit
                // close sockets and file log
                close(sockudp);
                close(sockfd);
                close(file_client);
                break;
            }

            // print server message
            printf("%s\n", buffer);

            // write to file log
            write(file_client, buffer, strlen(buffer));
            write(file_client, "\n", 1);
        }
    }

    return 0;
}
