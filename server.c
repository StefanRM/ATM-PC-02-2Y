/*
 * Protocoale de comunicatii
 * Tema 2
 * ATM
 * server.c
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#define BUFLEN 1500
#define BACKLOG 30 // queue for server's clients

// needed information for a client
typedef struct {
    char lastname[13];
    char firstname[13];
    char card_nr[7];
    char pin[5];
    char secret_password[17];
    double sold;
    int logged_in;
    int card_blocked;
} client_info;

// this function checks a client card in the clients array
// returns the client if the card was found, otherwise NULL
client_info* check_client_card(char* card_nr, client_info* clients, int n) {
    int i;
    for (i = 0; i < n; ++i)
    {
        if (strncmp(clients[i].card_nr, card_nr, 6) == 0)
        {
            return &clients[i];
        }
    }

    return NULL;
}

// this function gets the logged in client, given the client's socket
// returns the client if the client is logged in, otherwise NULL
client_info* get_client(int sockfd, client_info* clients, int n) {
    int i;
    for (i = 0; i < n; ++i)
    {
        if (clients[i].logged_in == sockfd)
        {
            return &clients[i];
        }
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int listenfd = 0, connfd = 0; // server and client socket
    char buffer[BUFLEN];
    struct sockaddr_in serv_addr, cli_addr; // server and client TCP addresses
    int error_check; // checking erros for called functions

    FILE* fp; // users file data
    int n, i; // n - number of clients

    fd_set read_fds; // reading set used in select()
    fd_set tmp_fds; // temporary used set
    int fdmax; // file descriptor's maximum value in read_fds set

    if(argc != 3) // wrong number of parameters
    {
        printf("-10 : Eroare la apelul executabilului\n");
        exit(0);
    }

    // users_data_file handling
    fp = fopen(argv[2], "rt");
    fscanf(fp, "%d", &n);

    // allocating clients array
    client_info* clients = (client_info*) calloc(n, sizeof(client_info));

    // reading each client's information
    for (i = 0; i < n; ++i)
    {
        fscanf(fp, "%s", clients[i].lastname);
        fscanf(fp, "%s", clients[i].firstname);
        fscanf(fp, "%s", clients[i].card_nr);
        fscanf(fp, "%s", clients[i].pin);
        fscanf(fp, "%s", clients[i].secret_password);
        fscanf(fp, "%lf", &(clients[i].sold));
        clients[i].logged_in = 0;
        clients[i].card_blocked = 0;
    }

    fclose(fp);
    // end of users_data_file handling

    // we want the reading descriptors' set (read_fds) and tmp_fds empty
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    // open server socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        printf("-10 : Eroare la apel socket() - TCP\n");
        exit(0);
    }

    // server addres information
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(atoi(argv[1]));

    // bind properties to socket
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("-10 : Eroare la apel bind() - TCP\n");
        exit(0);
    }

    // start listening for connections
    listen(listenfd, BACKLOG);

    // adding new file descriptor (server socket) in read_fds set
    FD_SET(listenfd, &read_fds);
    FD_SET(0, &read_fds); // adding stdin
    fdmax = listenfd;

    // UDP declare
    int sockudp; // UDP socket
    // server and client UDP addresses
    struct sockaddr_in serv_udp_addr, cli_udp_addr;
    unsigned int cli_udp_len = sizeof(cli_udp_addr);
    sockudp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockudp < 0)
    {
        printf("-10 : Eroare la apel socket() - UDP\n");
        exit(0);
    }

    // UDP server addres information
    memset(&serv_udp_addr, 0, sizeof(serv_udp_addr));
    serv_udp_addr.sin_family = AF_INET;
    serv_udp_addr.sin_port = htons(atoi(argv[1]));
    serv_udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockudp, (struct sockaddr *)&serv_udp_addr, sizeof(serv_udp_addr)) < 0)
    {
        printf("-10 : Eroare la apel bind() - UDP\n");
        exit(0);
    }

    // in case of sockudp being the maximum file descriptor in read_fds
    if (fdmax < sockudp)
    {
        fdmax = sockudp;
    }

    FD_SET(sockudp, &read_fds); // adding UDP socket
    // end of UDP declare

    unsigned int clilen; // client address length
    char* token; // for tokenizer
    int poz = 0; // counter for tokens
    char card_nr[7]; // read value of card_nr
    char pin[5]; // read value of pin
    client_info* curr_client; // current client
    char sold[25]; // read value of sold
    int money_to_take;
    double money_to_put;
    int j, len; // j - contor, len - for string handling after fgets()
    char secret_password[17]; // read value of secret password
    int attempt_to_unlock = 0; // if the client attempts to unlock card

    // main loop
    while (1)
    {
        tmp_fds = read_fds;

        // multiplex
        if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) < 0)
        {
            printf("-10 : Eroare la apel select()\n");
            exit(0);
        }
    
        // check all descriptors
        for(i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &tmp_fds))
            { // it is used
                if (i == 0) // stdin
                {
                    memset(buffer, 0, BUFLEN);
                    fgets(buffer, BUFLEN-1, stdin); // read
                    len = strlen(buffer);
                    if (buffer[len - 1] == '\n') // get rid of '\n'
                    {
                        buffer[len - 1] = '\0';
                    }

                    if (strncmp(buffer, "quit", 4) == 0) // quit server
                    {
                        // clients' sockets begin from socket 4
                        for (j = 4; j <=fdmax; ++j)
                        {
                            if (j != sockudp) // just clients' sockets
                            {
                                // send quit message
                                send(j, buffer, BUFLEN, 0);
                                FD_CLR(j, &read_fds);
                            }
                        }

                        close(sockudp);
                        FD_CLR(sockudp, &read_fds);
                        close(listenfd);
                        return 0;
                    }
                }

                // UDP connection
                else if (i == sockudp)
                {
                    memset(buffer, 0, BUFLEN);
                    // receive message from UDP connection
                    recvfrom(sockudp, buffer, BUFLEN, 0, (struct sockaddr*)&cli_udp_addr, &cli_udp_len);
                    printf ("Am primit de la clientul de pe socketul UDP, mesajul: %s\n", buffer);

                    if ((strncmp(buffer, "unlock", 6) == 0) && (attempt_to_unlock == 0)) // unlock command
                    {
                        memset(card_nr, 0, 7);

                        // get card number from message
                        token = strtok(buffer, " ");
                        poz = 0;
                        while (token != NULL)
                        {
                            if (poz == 1)
                            {
                                strncpy(card_nr, token, 6);
                            }
                            token = strtok(NULL, " ");
                            poz++;
                        }

                        memset(buffer, 0, BUFLEN);

                        // get current client using the given card_nr
                        if((curr_client = check_client_card(card_nr, clients, n)) == NULL)
                        {
                            strcpy(buffer, "UNLOCK> -4 : Numar card inexistent");
                            send(i, buffer, BUFLEN, 0);
                        } else { // it exists
                            if (curr_client->card_blocked < 3)
                            {
                                strcpy(buffer, "UNLOCK> -6 : Operatie esuata");
                            } else { // card is blocked
                                strcpy(buffer, "UNLOCK> Trimite parola secreta");
                                attempt_to_unlock = 1; // mark that next comes the secret password
                            }
                        }

                        // send response via UDP connection
                        sendto(sockudp, buffer, BUFLEN, 0, (struct sockaddr *)&cli_udp_addr, sizeof(cli_udp_addr));

                    } else { // second message after unlock command
                        memset(card_nr, 0, 7);
                        memset(secret_password, 0, 17);

                        // get card number and secret password from message
                        token = strtok(buffer, " ");
                        poz = 0;
                        while (token != NULL)
                        {
                            if (poz == 0)
                            {
                                strncpy(card_nr, token, 6);
                            }
                            if (poz == 1)
                            {
                                strcpy(secret_password, token);
                            }
                            token = strtok(NULL, " ");
                            poz++;
                        }

                        memset(buffer, 0, BUFLEN);

                        // get current client using the given card_nr
                        if((curr_client = check_client_card(card_nr, clients, n)) == NULL)
                        {
                            strcpy(buffer, "UNLOCK> -4 : Numar card inexistent");
                            send(i, buffer, BUFLEN, 0);
                        } else { // it exists
                            if (curr_client->card_blocked < 3)
                            {
                                strcpy(buffer, "UNLOCK> -6 : Operatie esuata");
                            } else { // card is blocked
                                if (strcmp(curr_client->secret_password, secret_password) == 0)
                                {
                                    curr_client->card_blocked = 0;
                                    strcpy(buffer, "UNLOCK> Client deblocat");
                                } else { // wrong secret password
                                    strcpy(buffer, "UNLOCK> -7 : Deblocare esuata");
                                }
                            }
                        }

                        attempt_to_unlock = 0; // second step of unlockin is over

                        // send response via UDP connection
                        sendto(sockudp, buffer, BUFLEN, 0, (struct sockaddr *)&cli_udp_addr, sizeof(cli_udp_addr));
                    }
                }
                // end of UDP connection
            
                else if (i == listenfd)
                {
                    // something came on the inactive socket (server socket)
                    // we have a new connection (a new client)
                    // accepting client
                    clilen = sizeof(cli_addr);
                    if ((connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clilen)) < 0)
                    {
                        printf("-10 : Eroare la apel accept()\n");
                        exit(0);
                    } else {
                        // client socket added to the descriptor set
                        FD_SET(connfd, &read_fds);

                        // in case of connfd being the maximum file descriptor in read_fds
                        if (connfd > fdmax)
                        { 
                            fdmax = connfd;
                        }
                    }
                    printf("Noua conexiune de la %s, port %d, socket_client %d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), connfd);
                } else {
                    // message incoming on one of the active client sockets
                    memset(buffer, 0, BUFLEN);
                    if ((error_check = recv(i, buffer, sizeof(buffer), 0)) <= 0)
                    {
                        if (error_check < 0)
                        {
                            printf("-10 : Eroare la apel recv()\n");
                        }
                        close(i); 
                        FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care 
                    } else { // message received
                        printf ("Am primit de la clientul de pe socketul %d, mesajul: %s\n", i, buffer);

                        // check client's given command
                        if (strncmp(buffer, "login", 5) == 0) // login command
                        {
                            memset(card_nr, 0, 7);
                            memset(pin, 0, 5);

                            // get card number and pin from the message
                            token = strtok(buffer, " ");
                            poz = 0;
                            while (token != NULL)
                            {
                                if (poz == 1)
                                {
                                    strncpy(card_nr, token, 6);
                                }
                                if (poz == 2)
                                {
                                    strncpy(pin, token, 4);
                                }
                                token = strtok(NULL, " ");
                                poz++;
                            }

                            memset(buffer, 0, BUFLEN);

                            // get current client using the given card_nr
                            if((curr_client = check_client_card(card_nr, clients, n)) == NULL)
                            {
                                strcpy(buffer, "ATM> -4 : Numar card inexistent");
                                send(i, buffer, BUFLEN, 0);
                            } else { // it exists
                                if (curr_client->logged_in >= 1)
                                { // already logged in
                                    // sending response to client
                                    strcpy(buffer, "ATM> -2 : Sesiune deja deschisa");
                                    send(i, buffer, BUFLEN, 0);
                                }
                                else if (curr_client->card_blocked >= 3)
                                { // card blocked
                                    // sending response to client
                                    strcpy(buffer, "ATM> -5 : Card blocat");
                                    send(i, buffer, BUFLEN, 0);
                                }
                                else
                                { // card not blocked
                                    if (strncmp(curr_client->pin, pin, 4) == 0) 
                                    { // correct pin
                                        // sending response to client
                                        strcpy(buffer, "ATM> Welcome ");
                                        strcat(buffer, curr_client->lastname);
                                        strcat(buffer, " ");
                                        strcat(buffer, curr_client->firstname);
                                        // card blocked counter reset
                                        curr_client->card_blocked = 0;
                                        // client logged in set
                                        curr_client->logged_in = i;
                                        send(i, buffer, BUFLEN, 0);
                                    } else { // incorrect pin
                                        // sending response to client
                                        strcpy(buffer, "ATM> -3 : Pin gresit");

                                        // card blocked counter updated
                                        curr_client->card_blocked++;
                                        if (curr_client->card_blocked >= 3)
                                        {
                                            memset(buffer, 0, BUFLEN);
                                            strcpy(buffer, "ATM> -5 : Card blocat");
                                        }
                                        send(i, buffer, BUFLEN, 0);
                                    }
                                }
                            }
                        }
                        else if (strncmp(buffer, "logout", 6) == 0) // logout command
                        {
                            // get current client logged in with client's socket
                            if ((curr_client = get_client(i, clients, n)) == NULL){
                                // something wrong happened
                                printf("-10 : Eroare la logout\n");
                            } else {
                                // send response to client
                                memset(buffer, 0, BUFLEN);
                                strcpy(buffer, "ATM> Deconectare de la bancomat");
                                curr_client->logged_in = 0; // unset logged in
                                send(i, buffer, BUFLEN, 0);
                            }
                        }
                        else if (strncmp(buffer, "listsold", 8) == 0) // listsold command
                        {
                            // get current client logged in with client's socket
                            if ((curr_client = get_client(i, clients, n)) == NULL){
                                // something wrong happened
                                printf("-10 : Eroare la listsold\n");
                            } else {
                                // send response to client
                                memset(sold, 0, 25);
                                sprintf(sold, "%.2lf", curr_client->sold);
                                memset(buffer, 0, BUFLEN);
                                strcpy(buffer, "ATM> ");
                                strcat(buffer, sold);
                                send(i, buffer, BUFLEN, 0);
                            }
                        }
                        else if (strncmp(buffer, "getmoney", 8) == 0) // getmoney command
                        {
                            // get current client logged in with client's socket
                            if ((curr_client = get_client(i, clients, n)) == NULL){
                                // something wrong happened
                                printf("-10 : Eroare la getmoney\n");
                            } else {
                                memset(sold, 0, 25);

                                // get sold from the message
                                token = strtok(buffer, " ");
                                poz = 0;
                                while (token != NULL)
                                {
                                    if (poz == 1)
                                    {
                                        strcpy(sold, token);
                                    }
                                    token = strtok(NULL, " ");
                                    poz++;
                                }

                                // convert to integer the sold value
                                money_to_take = atoi(sold);
                                memset(buffer, 0, BUFLEN);

                                if (money_to_take % 10 != 0)
                                {
                                    // send response to client
                                    strcpy(buffer, "ATM> -9 : Suma nu e multiplu de 10");
                                    send(i, buffer, BUFLEN, 0);
                                } else if (money_to_take > curr_client->sold)
                                {
                                    // send response to client
                                    strcpy(buffer, "ATM> -8 : Fonduri insuficiente");
                                    send(i, buffer, BUFLEN, 0);
                                } else {
                                    // correct input sold
                                    // updating client's sold
                                    curr_client->sold -= money_to_take;

                                    // send response to client
                                    strcpy(buffer, "ATM> Suma ");
                                    strcat(buffer, sold);
                                    strcat(buffer, " retrasa cu succes");
                                    send(i, buffer, BUFLEN, 0);
                                }
                            }
                        }
                        else if (strncmp(buffer, "putmoney", 8) == 0) // putmoney command
                        {
                            // get current client logged in with client's socket
                            if ((curr_client = get_client(i, clients, n)) == NULL){
                                // something wrong happened
                                printf("-10 : Eroare la putmoney\n");
                            } else {
                                memset(sold, 0, 25);

                                // get sold from the message
                                token = strtok(buffer, " ");
                                poz = 0;
                                while (token != NULL)
                                {
                                    if (poz == 1)
                                    {
                                        strcpy(sold, token);
                                    }
                                    token = strtok(NULL, " ");
                                    poz++;
                                }

                                // convert to double the sold value
                                sscanf(sold, "%lf", &money_to_put);
                                memset(buffer, 0, BUFLEN);

                                // updating client's sold
                                curr_client->sold += money_to_put;

                                // send response to client
                                strcpy(buffer, "ATM> Suma depusa cu succes");
                                send(i, buffer, BUFLEN, 0);
                            }
                        }
                        else if (strncmp(buffer, "quit", 4) == 0) // quit command
                        {
                            // check if the client is logged in
                            if ((curr_client = get_client(i, clients, n)) == NULL){
                                printf("Clientul %d nu era logat\n", i);
                            } else {
                                curr_client->logged_in = 0;
                                printf("Clientul %d a fost delogat\n", i);
                            }

                            // close connection
                            printf("Clientul %d a inchis conexiunea\n", i);
                            FD_CLR(i, &read_fds);
                        }
                    }
                } 
            }
        }
     }
}
