/*
 * protocol.h
 *
 * Shared header file for UDP client and server
 * Contains protocol definitions, data structures, constants and function prototypes
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stdint.h>

/*
 * ============================================================================
 * PROTOCOL CONSTANTS
 * ============================================================================
 */

#define DEFAULT_PORT 56700
#define CITY_SIZE 64
#define BUFFER_SIZE 512

// Request types
#define REQ_TEMPERATURE 't'
#define REQ_HUMIDITY 'h'
#define REQ_WIND 'w'
#define REQ_PRESSURE 'p'

// Response status codes
#define STATUS_SUCCESS 0
#define STATUS_CITY_NOT_FOUND 1
#define STATUS_INVALID_REQUEST 2

// Request buffer size: type (1 byte) + city (64 bytes)
#define REQUEST_BUFFER_SIZE (sizeof(char) + CITY_SIZE)

// Response buffer size: status (4 bytes) + type (1 byte) + value (4 bytes)
#define RESPONSE_BUFFER_SIZE (sizeof(uint32_t) + sizeof(char) + sizeof(float))

/*
 * ============================================================================
 * PROTOCOL DATA STRUCTURES
 * ============================================================================
 */

// Weather request structure
struct request {
    char type;      // 't'=temperatura, 'h'=umidità, 'w'=vento, 'p'=pressione
    char city[CITY_SIZE];  // nome città (null-terminated)
};

// Weather response structure
struct response {
    unsigned int status;  // 0=successo, 1=città non trovata, 2=richiesta invalida
    char type;            // eco del tipo richiesto
    float value;          // dato meteo generato
};

/*
 * ============================================================================
 * FUNCTION PROTOTYPES
 * ============================================================================
 */

// Serialization functions
int serialize_request(const struct request *req, char *buffer);
int deserialize_request(const char *buffer, struct request *req);
int serialize_response(const struct response *resp, char *buffer);
int deserialize_response(const char *buffer, struct response *resp);

// Validation functions
int is_valid_request_type(char type);
int contains_invalid_chars(const char *str);

// Utility functions
void to_lowercase(char *str);
void capitalize_city(char *city);


#endif /* PROTOCOL_H_ */
