/*
 * main.c
 *
 * UDP Server - Template for Computer Networks assignment
 *
 * This file contains the boilerplate code for a UDP server
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
#include <time.h>
#include <ctype.h>
#include "protocol.h"

#define NO_ERROR 0
#define NUM_CITIES 10

static const char *supported_cities[NUM_CITIES] = {
    "bari", "roma", "milano", "napoli", "torino",
    "palermo", "genova", "bologna", "firenze", "venezia"
};

void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}


void to_lowercase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

void capitalize_city(char *city) {
    int capitalize_next = 1;
    for (int i = 0; city[i]; i++) {
        if (city[i] == ' ') {
            capitalize_next = 1;
        } else if (capitalize_next) {
            city[i] = toupper((unsigned char)city[i]);
            capitalize_next = 0;
        } else {
            city[i] = tolower((unsigned char)city[i]);
        }
    }
}

int is_valid_request_type(char type) {
    return (type == REQ_TEMPERATURE || type == REQ_HUMIDITY ||
            type == REQ_WIND || type == REQ_PRESSURE);
}

int contains_invalid_chars(const char *str) {
    for (int i = 0; str[i]; i++) {
        char c = str[i];
        // Allow letters, digits, spaces, accented characters (negative values in signed char)
        if (c == '\t') return 1; // Tab not allowed
        if (c == '@' || c == '#' || c == '$' || c == '%' || c == '^' ||
            c == '&' || c == '*' || c == '(' || c == ')' || c == '!' ||
            c == '~' || c == '`' || c == '+' || c == '=' || c == '[' ||
            c == ']' || c == '{' || c == '}' || c == '|' || c == '\\' ||
            c == '<' || c == '>' || c == '?' || c == '/' || c == ';' ||
            c == ':' || c == '"') {
            return 1;
        }
    }
    return 0;
}

int is_city_supported(const char *city) {
    char city_lower[CITY_SIZE];
    strncpy(city_lower, city, CITY_SIZE - 1);
    city_lower[CITY_SIZE - 1] = '\0';
    to_lowercase(city_lower);

    for (int i = 0; i < NUM_CITIES; i++) {
        if (strcmp(city_lower, supported_cities[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

float get_temperature() {
    return -10.0f + ((float)rand() / RAND_MAX) * 50.0f;
}

float get_humidity() {
    return 20.0f + ((float)rand() / RAND_MAX) * 80.0f;
}

float get_wind() {
    return ((float)rand() / RAND_MAX) * 100.0f;
}

float get_pressure() {
    return 950.0f + ((float)rand() / RAND_MAX) * 100.0f;
}

int serialize_request(const struct request *req, char *buffer) {
    int offset = 0;

    // Type (1 byte, no conversion needed)
    memcpy(buffer + offset, &req->type, sizeof(char));
    offset += sizeof(char);

    // City (64 bytes)
    memcpy(buffer + offset, req->city, CITY_SIZE);
    offset += CITY_SIZE;

    return offset;
}

int deserialize_request(const char *buffer, struct request *req) {
    int offset = 0;

    // Type (1 byte)
    memcpy(&req->type, buffer + offset, sizeof(char));
    offset += sizeof(char);

    // City (64 bytes)
    memcpy(req->city, buffer + offset, CITY_SIZE);
    req->city[CITY_SIZE - 1] = '\0'; // Ensure null-termination
    offset += CITY_SIZE;

    return offset;
}

int serialize_response(const struct response *resp, char *buffer) {
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

int deserialize_response(const char *buffer, struct response *resp) {
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

void get_hostname_from_ip(struct sockaddr_in *addr, char *hostname, size_t hostname_len, char *ip_str, size_t ip_len) {
    // Get IP string
    inet_ntop(AF_INET, &(addr->sin_addr), ip_str, ip_len);

    // Reverse DNS lookup
    struct hostent *host = gethostbyaddr((const char *)&(addr->sin_addr), sizeof(addr->sin_addr), AF_INET);
    if (host != NULL && host->h_name != NULL) {
        strncpy(hostname, host->h_name, hostname_len - 1);
        hostname[hostname_len - 1] = '\0';
    } else {
        strncpy(hostname, ip_str, hostname_len - 1);
        hostname[hostname_len - 1] = '\0';
    }
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        }
    }

#if defined WIN32
    // Initialize Winsock
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != NO_ERROR) {
        printf("Error at WSAStartup()\n");
        return 1;
    }
#endif

    // Seed random number generator
    srand((unsigned int)time(NULL));

    int my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (my_socket < 0) {
        printf("Errore nella creazione del socket\n");
        clearwinsock();
        return 1;
    }

    // Configure server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket
    if (bind(my_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Errore nel bind del socket\n");
        closesocket(my_socket);
        clearwinsock();
        return 1;
    }

    printf("Server UDP in ascolto sulla porta %d...\n", port);

    while (1) {
        char recv_buffer[BUFFER_SIZE];
        char send_buffer[BUFFER_SIZE];
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int recv_len = recvfrom(my_socket, recv_buffer, sizeof(recv_buffer), 0,
                                (struct sockaddr *)&client_addr, &client_addr_len);

        if (recv_len < 0) {
            printf("Errore nella ricezione\n");
            continue;
        }

        // Get client hostname and IP for logging
        char client_hostname[256];
        char client_ip[INET_ADDRSTRLEN];
        get_hostname_from_ip(&client_addr, client_hostname, sizeof(client_hostname),
                             client_ip, sizeof(client_ip));

        struct request req;
        deserialize_request(recv_buffer, &req);

        printf("Richiesta ricevuta da %s (ip %s): type='%c', city='%s'\n",
               client_hostname, client_ip, req.type, req.city);

        struct response resp;
        resp.type = req.type;
        resp.value = 0.0f;

        if (!is_valid_request_type(req.type)) {
            resp.status = STATUS_INVALID_REQUEST;
        }
        else if (contains_invalid_chars(req.city)) {
            resp.status = STATUS_INVALID_REQUEST;
        }
        else if (!is_city_supported(req.city)) {
            resp.status = STATUS_CITY_NOT_FOUND;
        }
        // Generate weather data
        else {
            resp.status = STATUS_SUCCESS;
            switch (req.type) {
                case REQ_TEMPERATURE:
                    resp.value = get_temperature();
                    break;
                case REQ_HUMIDITY:
                    resp.value = get_humidity();
                    break;
                case REQ_WIND:
                    resp.value = get_wind();
                    break;
                case REQ_PRESSURE:
                    resp.value = get_pressure();
                    break;
            }
        }

        int send_len = serialize_response(&resp, send_buffer);
        sendto(my_socket, send_buffer, send_len, 0,
               (struct sockaddr *)&client_addr, client_addr_len);
    }

    printf("Server terminated.\n");

    closesocket(my_socket);
    clearwinsock();
    return 0;
}
