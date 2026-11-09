#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "checksum.h"
#include "Crc.h"

#define PORT 4455
#define HEADER_SIZE 16
#define PAYLOAD_SIZE 46
#define MAX_BUFFER_SIZE 100

#pragma pack(1)
typedef struct {
    unsigned char src_mac[6];
    unsigned char dest_mac[6];
    unsigned short length;
    unsigned short seq_no; // <--- UPGRADED to 16-bit (2 bytes)
} FrameHeader;

typedef struct {
    unsigned short ack_no; // <--- UPGRADED to 16-bit (2 bytes)
} AckFrame;
#pragma pack()

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    unsigned char buffer[MAX_BUFFER_SIZE];

    int crc_choice;
    printf("======== CRC ========:\n");
    printf("1. CRC 8\n2. CRC 10\n3. CRC 16\n4. CRC 32\nChoice: ");
    scanf("%d", &crc_choice);
    if (crc_choice != 8 && crc_choice != 10 && crc_choice != 16 && crc_choice != 32) crc_choice = 16;

    int length = HEADER_SIZE + PAYLOAD_SIZE + 2; // Header + Payload + Checksum
    switch(crc_choice) {
        case 8: length += 1; break;
        case 10:
        case 16: length += 2; break;
        case 32: length += 4; break;
    }

    int arq_choice;
    printf("\n------- Flow Control Protocol -------\n");
    printf("1. Stop and Wait ARQ\n");
    printf("2. Go-Back-N ARQ\n");
    printf("3. Selective Repeat ARQ\n");
    printf("Enter expected protocol: ");
    scanf("%d", &arq_choice);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    
    printf("Server listening on port %d...\n\n", PORT);
    unsigned short expected_seq = 0; // Stop and Wait tracker

    while (1) {
        int bytes_received = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        
        if (bytes_received == 4 && strncmp((char *)buffer, "DONE", 4) == 0) {
            printf("\n[Server] Received DONE signal. Ending transmission...\n");
            break; 
        }

        if(bytes_received == length) {
            unsigned short chk_result = cal_chksum((unsigned short *)buffer, HEADER_SIZE + PAYLOAD_SIZE + 2);
            int chk_error = (chk_result != 0x0000);

            int crc_error = 0;
            uint32_t dis_calc_crc = 0, dis_recv_crc = 0;
            int base_len = HEADER_SIZE + PAYLOAD_SIZE;

            switch(crc_choice) {
                case 8:
                    dis_calc_crc = cal_crc8(buffer, base_len);
                    dis_recv_crc = buffer[base_len + 2]; 
                    crc_error = (dis_calc_crc != dis_recv_crc);
                    break;
                case 10:
                    dis_calc_crc = cal_crc10(buffer, base_len);
                    dis_recv_crc = buffer[base_len + 2] | (buffer[base_len + 3] << 8); 
                    crc_error = (dis_calc_crc != dis_recv_crc);
                    break;
                case 16:
                    dis_calc_crc = cal_crc16(buffer, base_len);
                    dis_recv_crc = buffer[base_len + 2] | (buffer[base_len + 3] << 8); 
                    crc_error = (dis_calc_crc != dis_recv_crc);
                    break;
                case 32:
                    dis_calc_crc = cal_crc32(buffer, base_len);
                    dis_recv_crc = (uint32_t)buffer[base_len + 2] | ((uint32_t)buffer[base_len + 3] << 8) | ((uint32_t)buffer[base_len + 4] << 16) | ((uint32_t)buffer[base_len + 5] << 24); 
                    crc_error = (dis_calc_crc != dis_recv_crc);
                    break;
            }

            FrameHeader *hdr = (FrameHeader *)buffer;
            unsigned short received_seq = hdr->seq_no;

            if (chk_error || crc_error) {
                printf("[REJECTED] Error in Sequence %d! Frame discarded.\n", received_seq);
                // Deliberately DO NOT send an ACK here[cite: 40].
           } else {
                AckFrame ack;

                if (arq_choice == 1) { 
                    // Stop and Wait Logic
                    if (received_seq == expected_seq) {
                        printf("○ Sequence %d accepted (In-Order).\n", received_seq);
                        ack.ack_no = expected_seq;
                        expected_seq = (expected_seq + 1) % 2; 
                    } else {
                        printf("○ Sequence %d DISCARDED (Duplicate).\n", received_seq);
                        ack.ack_no = (expected_seq == 0) ? 1 : 0; 
                    }
                } 
                else if (arq_choice == 2) { 
                    // Go-Back-N Logic
                    if (received_seq == expected_seq) {
                        printf("○ Sequence %d accepted (In-Order).\n", received_seq);
                        ack.ack_no = expected_seq;
                        expected_seq++; 
                    } else {
                        printf("○ Sequence %d DISCARDED (Out-of-Order). Expected %d.\n", received_seq, expected_seq);
                        ack.ack_no = expected_seq - 1; 
                    }
                } 
                else { 
                    // Selective Repeat Logic
                    ack.ack_no = received_seq; 
                    if (received_seq == expected_seq) {
                        printf("○ Sequence %d accepted (In-Order).\n", received_seq);
                        expected_seq++; 
                    } else if (received_seq > expected_seq) {
                        printf("○ Sequence %d accepted (Out-of-Order, Buffered).\n", received_seq);
                    } else {
                        printf("○ Sequence %d received (Duplicate).\n", received_seq);
                    }
                }

                sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, addr_len);
                printf("-> Sent ACK %d.\n", ack.ack_no);
            }
        }
    }
    close(sockfd);
    return 0;
}