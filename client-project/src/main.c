/*
 * main.c
 *
 * UDP Client - Template for Computer Networks assignment
 *
 * This file contains the boilerplate code for a UDP client
 * portable across Windows, Linux, and macOS.
 */

#if defined WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define closesocket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "protocol.h"

#define NO_ERROR 0

void clearwinsock()
{
#if defined WIN32
    WSACleanup();
#endif
}

void to_lowercase(char *str)
{
    for (int i = 0; str[i]; i++)
    {
        str[i] = tolower((unsigned char)str[i]);
    }
}

void capitalize_city(char *city)
{
    int capitalize_next = 1;
    for (int i = 0; city[i]; i++)
    {
        if (city[i] == ' ')
        {
            capitalize_next = 1;
        }
        else if (capitalize_next)
        {
            city[i] = toupper((unsigned char)city[i]);
            capitalize_next = 0;
        }
        else
        {
            city[i] = tolower((unsigned char)city[i]);
        }
    }
}

int is_valid_request_type(char type)
{
    return (type == REQ_TEMPERATURE || type == REQ_HUMIDITY ||
            type == REQ_WIND || type == REQ_PRESSURE);
}

int contains_invalid_chars(const char *str)
{
    for (int i = 0; str[i]; i++)
    {
        if (str[i] == '\t')
            return 1; // Tab not allowed
    }
    return 0;
}

int serialize_request(const struct request *req, char *buffer)
{
    int offset = 0;

    // Type (1 byte, no conversion needed)
    memcpy(buffer + offset, &req->type, sizeof(char));
    offset += sizeof(char);

    // City (64 bytes)
    memcpy(buffer + offset, req->city, CITY_SIZE);
    offset += CITY_SIZE;

    return offset;
}

int deserialize_request(const char *buffer, struct request *req)
{
    int offset = 0;

    // Type (1 byte)
    memcpy(&req->type, buffer + offset, sizeof(char));
    offset += sizeof(char);

    // City (64 bytes)
    memcpy(req->city, buffer + offset, CITY_SIZE);
    req->city[CITY_SIZE - 1] = '\0';
    offset += CITY_SIZE;

    return offset;
}

int serialize_response(const struct response *resp, char *buffer)
{
    int offset = 0;

    // Status (4 bytes with network byte order)
    uint32_t net_status = htonl(resp->status);
    memcpy(buffer + offset, &net_status, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Type (1 byte, no conversion needed)
    memcpy(buffer + offset, &resp->type, sizeof(char));
    offset += sizeof(char);

    // Value (float with network byte order)
    uint32_t temp;
    memcpy(&temp, &resp->value, sizeof(float));
    temp = htonl(temp);
    memcpy(buffer + offset, &temp, sizeof(float));
    offset += sizeof(float);

    return offset;
}

int deserialize_response(const char *buffer, struct response *resp)
{
    int offset = 0;

    // Status (4 bytes)
    uint32_t net_status;
    memcpy(&net_status, buffer + offset, sizeof(uint32_t));
    resp->status = ntohl(net_status);
    offset += sizeof(uint32_t);

    // Type (1 byte)
    memcpy(&resp->type, buffer + offset, sizeof(char));
    offset += sizeof(char);

    // Value (float)
    uint32_t temp;
    memcpy(&temp, buffer + offset, sizeof(float));
    temp = ntohl(temp);
    memcpy(&resp->value, &temp, sizeof(float));
    offset += sizeof(float);

    return offset;
}

int resolve_hostname(const char *hostname, struct in_addr *addr)
{
    // First try to parse as IP address
    if (inet_pton(AF_INET, hostname, addr) == 1)
    {
        return 1; // Already an IP address
    }

    // Try DNS lookup
    struct hostent *host = gethostbyname(hostname);
    if (host == NULL)
    {
        return 0;
    }

    memcpy(addr, host->h_addr_list[0], sizeof(struct in_addr));
    return 1;
}

void get_hostname_from_ip(struct in_addr *addr, char *hostname, size_t hostname_len, char *ip_str, size_t ip_len)
{
    // Get IP string
    inet_ntop(AF_INET, addr, ip_str, ip_len);

    struct hostent *host = gethostbyaddr((const char *)addr, sizeof(struct in_addr), AF_INET);
    if (host != NULL && host->h_name != NULL)
    {
        strncpy(hostname, host->h_name, hostname_len - 1);
        hostname[hostname_len - 1] = '\0';
    }
    else
    {
        strncpy(hostname, ip_str, hostname_len - 1);
        hostname[hostname_len - 1] = '\0';
    }
}

int parse_request_string(const char *request_str, struct request *req)
{
    const char *space = strchr(request_str, ' ');
    if (space == NULL)
    {
        printf("Errore: formato richiesta non valido (manca lo spazio tra type e city)\n");
        return -1;
    }

    // Check if type is a single character
    int type_len = space - request_str;
    if (type_len != 1)
    {
        printf("Errore: il tipo deve essere un singolo carattere\n");
        return -1;
    }

    req->type = request_str[0];

    // Get city (skip the space)
    const char *city_start = space + 1;

    // Skip leading spaces
    while (*city_start == ' ')
    {
        city_start++;
    }

    if (*city_start == '\0')
    {
        printf("Errore: nome città mancante\n");
        return -1;
    }

    // Check city length
    size_t city_len = strlen(city_start);
    if (city_len >= CITY_SIZE)
    {
        printf("Errore: nome città troppo lungo (max 63 caratteri)\n");
        return -1;
    }

    // Copy city
    strncpy(req->city, city_start, CITY_SIZE - 1);
    req->city[CITY_SIZE - 1] = '\0';

    return 0;
}

void print_result(const char *server_name, const char *server_ip,
                  const struct response *resp, const char *city)
{
    if (resp->status == STATUS_SUCCESS)
    {
        // Create a copy of city for capitalization
        char city_formatted[CITY_SIZE];
        strncpy(city_formatted, city, CITY_SIZE - 1);
        city_formatted[CITY_SIZE - 1] = '\0';
        capitalize_city(city_formatted);

        switch (resp->type)
        {
        case REQ_TEMPERATURE:
            printf("Ricevuto risultato dal server %s (ip %s). %s: Temperatura = %.1f°C\n",
                   server_name, server_ip, city_formatted, resp->value);
            break;
        case REQ_HUMIDITY:
            printf("Ricevuto risultato dal server %s (ip %s). %s: Umidità = %.1f%%\n",
                   server_name, server_ip, city_formatted, resp->value);
            break;
        case REQ_WIND:
            printf("Ricevuto risultato dal server %s (ip %s). %s: Vento = %.1f km/h\n",
                   server_name, server_ip, city_formatted, resp->value);
            break;
        case REQ_PRESSURE:
            printf("Ricevuto risultato dal server %s (ip %s). %s: Pressione = %.1f hPa\n",
                   server_name, server_ip, city_formatted, resp->value);
            break;
        }
    }
    else if (resp->status == STATUS_CITY_NOT_FOUND)
    {
        printf("Ricevuto risultato dal server %s (ip %s). Città non disponibile\n",
               server_name, server_ip);
    }
    else if (resp->status == STATUS_INVALID_REQUEST)
    {
        printf("Ricevuto risultato dal server %s (ip %s). Richiesta non valida\n",
               server_name, server_ip);
    }
}

int main(int argc, char *argv[])
{
    char *server = "localhost";
    int port = DEFAULT_PORT;
    char *request_str = NULL;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
        {
            server = argv[++i];
        }
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
        {
            port = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc)
        {
            request_str = argv[++i];
        }
    }

    if (request_str == NULL)
    {
        printf("Uso: %s [-s server] [-p port] -r \"type city\"\n", argv[0]);
        printf("  -s server: hostname o IP del server (default: localhost)\n");
        printf("  -p port: porta del server (default: %d)\n", DEFAULT_PORT);
        printf("  -r request: richiesta meteo (obbligatoria)\n");
        printf("  type: t=temperatura, h=umidità, w=vento, p=pressione\n");
        return 1;
    }

#if defined WIN32
    // Initialize Winsock
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != NO_ERROR)
    {
        printf("Error at WSAStartup()\n");
        return 1;
    }
#endif

    // Parse request string
    struct request req;
    memset(&req, 0, sizeof(req));
    if (parse_request_string(request_str, &req) != 0)
    {
        clearwinsock();
        return 1;
    }

    int my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (my_socket < 0)
    {
        printf("Errore nella creazione del socket\n");
        clearwinsock();
        return 1;
    }

    struct in_addr server_addr_in;
    if (!resolve_hostname(server, &server_addr_in))
    {
        printf("Errore nella risoluzione del server: %s\n", server);
        closesocket(my_socket);
        clearwinsock();
        return 1;
    }

    char server_hostname[256];
    char server_ip[INET_ADDRSTRLEN];
    get_hostname_from_ip(&server_addr_in, server_hostname, sizeof(server_hostname),
                         server_ip, sizeof(server_ip));

    struct sockaddr_in server_sockaddr;
    memset(&server_sockaddr, 0, sizeof(server_sockaddr));
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_addr = server_addr_in;
    server_sockaddr.sin_port = htons(port);

    // Serialize request
    char send_buffer[BUFFER_SIZE];
    int send_len = serialize_request(&req, send_buffer);

    if (sendto(my_socket, send_buffer, send_len, 0,
               (struct sockaddr *)&server_sockaddr, sizeof(server_sockaddr)) < 0)
    {
        printf("Errore nell'invio della richiesta\n");
        closesocket(my_socket);
        clearwinsock();
        return 1;
    }

    char recv_buffer[BUFFER_SIZE];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    int recv_len = recvfrom(my_socket, recv_buffer, sizeof(recv_buffer), 0,
                            (struct sockaddr *)&from_addr, &from_len);
    if (recv_len < 0)
    {
        printf("Errore nella ricezione della risposta\n");
        closesocket(my_socket);
        clearwinsock();
        return 1;
    }

    // Deserialize response
    struct response resp;
    deserialize_response(recv_buffer, &resp);

    print_result(server_hostname, server_ip, &resp, req.city);

    closesocket(my_socket);
    clearwinsock();
    return 0;
}
