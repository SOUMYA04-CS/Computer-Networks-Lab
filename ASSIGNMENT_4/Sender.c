#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include <sys/time.h>
#include "checksum.h"
#include "walsh.h"

#define PORT 4455
#define PAYLOAD_SIZE 46
#define HEADER_SIZE 16
#define MAXIMUM_SIZE 100

int station_id;
int expected_senders;
int walsh_N;
int walsh_code[MAX_WALSH_SIZE];


void csma_sendto(int sockfd, const void *msg, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    if (len <= 5) { sendto(sockfd, msg, len, flags, dest_addr, addrlen); return; }
    
    int total_chips = len * 8 * walsh_N;
    int16_t *chips = (int16_t *)malloc(total_chips * sizeof(int16_t));
    
    const unsigned char *frame = (const unsigned char *)msg;
    int chip_idx = 0;
    
    for(size_t i = 0; i < len; i++) {
        unsigned char b = frame[i];
        for(int bit = 7; bit >= 0; bit--) {
            int bit_val = (b >> bit) & 1;
            int bipolar = (bit_val == 1) ? 1 : -1;
            for(int c = 0; c < walsh_N; c++) {
                chips[chip_idx++] = (int16_t)(bipolar * walsh_code[c]);
            }
        }
    }
    
    sendto(sockfd, chips, total_chips * sizeof(int16_t), flags, dest_addr, addrlen);
    free(chips);
    printf("Frame transmitted via CDMA (%d chips).\n", total_chips);
}

#pragma pack(1)
typedef struct {
    unsigned char src_mac[6]; unsigned char dest_mac[6];
    unsigned short length; unsigned short seq_no; 
} FrameHeader;
typedef struct { unsigned short ack_no; } AckFrame;
#pragma pack()

void injkterror(unsigned char *frame, int length, int error_choice) {} 
int Channel(float error_prob) { return 0; } 

#include "Crc.h"
#include "stop_wait.h"

int main(int argc, char *argv[]) {
    if(argc != 3) { printf("Usage: %s <Receiver_IP> <Test_File>\n", argv[0]); return 1; }

    printf("Enter Total Number of Senders (n): "); scanf("%d", &expected_senders);
    printf("Enter this Station's ID (0 to %d): ", expected_senders - 1); scanf("%d", &station_id);
    
    walsh_N = next_power_of_2(expected_senders);
    int walsh_table[MAX_WALSH_SIZE][MAX_WALSH_SIZE];
    generateWalshTable(walsh_N, walsh_table);
    for(int i=0; i<walsh_N; i++) walsh_code[i] = walsh_table[station_id][i];

    srand(time(NULL) ^ getpid()); 
    int sockfd = socket(PF_INET, SOCK_DGRAM, 0); 
    struct sockaddr_in server_addr;
    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    FILE *fp = fopen(argv[2], "rb");
    if(fp == NULL) { printf("File not found\n"); return 1; }

    int crc_choice, err_choice = 0; float error_prob = 0.0;
    printf("Choose CRC (8, 10, 16, 32): "); scanf("%d", &crc_choice);
    
    send_stop_and_wait(fp, &server_addr, sockfd, crc_choice, err_choice, error_prob);

    fclose(fp); close(sockfd); return 0;
}
