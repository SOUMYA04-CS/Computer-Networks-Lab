#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Crc.h"
#include "walsh.h"

#define PORT 4456 
#define HEADER_SIZE 16
#define PAYLOAD_SIZE 46
#define MAX_BUFFER_SIZE 65000 

#pragma pack(1)
typedef struct {
    unsigned char src_mac[6]; unsigned char dest_mac[6];
    unsigned short length; unsigned short seq_no; 
} FrameHeader;
typedef struct { unsigned short ack_no; } AckFrame;
#pragma pack()

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    unsigned char buffer[MAX_BUFFER_SIZE];

    int expected_senders = 1;
    printf("Enter number of contending senders expected: ");
    scanf("%d", &expected_senders);
    
    int walsh_N = next_power_of_2(expected_senders);
    int walsh_table[MAX_WALSH_SIZE][MAX_WALSH_SIZE];
    generateWalshTable(walsh_N, walsh_table);

    int crc_choice; printf("======== CRC ========:\n1. CRC 8\n10. CRC 10\n16. CRC 16\n32. CRC 32\nChoice: "); scanf("%d", &crc_choice);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; server_addr.sin_port = htons(PORT); server_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    
    printf("Server listening on port %d...\n\n", PORT);
    unsigned short expected_seq[64] = {0}; 
    int senders_finished = 0;

    while (1) {
        int bytes_received = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        
        if (bytes_received <= 10 && strncmp((char *)buffer, "DONE", 4) == 0) {
            senders_finished++;
            printf("\n[Server] Received DONE signal (%d/%d completed).\n", senders_finished, expected_senders);
            if (senders_finished >= expected_senders) break;
            continue;
        } 

        int16_t *composite = (int16_t *)buffer;
        int num_chips = bytes_received / sizeof(int16_t);
        int frame_bytes = num_chips / (8 * walsh_N);
        int ack_sent = 0;

        for (int st = 0; st < expected_senders; st++) {
            unsigned char reconstructed[MAX_BUFFER_SIZE];
            memset(reconstructed, 0, sizeof(reconstructed));
            
            int chip_idx = 0;
            for (int b = 0; b < frame_bytes && b < MAX_BUFFER_SIZE; b++) {
                for (int bit = 7; bit >= 0; bit--) {
                    int sum = 0;
                    for (int c = 0; c < walsh_N; c++) {
                        sum += composite[chip_idx++] * walsh_table[st][c];
                    }
                    int bipolar = sum / walsh_N;
                    if (bipolar > 0) reconstructed[b] |= (1 << bit);
                }
            }

        FrameHeader *hdr = (FrameHeader *)reconstructed;
        unsigned short actual_length = ntohs(hdr->length); 
        
        
        if (actual_length > 0 && actual_length <= PAYLOAD_SIZE) {
            unsigned short received_seq = hdr->seq_no;
            AckFrame ack;
            if (received_seq == expected_seq[st]) {
                printf("  [Station %d] Seq %d Accepted | Data: \"", st, received_seq);
                
                
                for (int i = 0; i < actual_length; i++) {
                    char c = reconstructed[HEADER_SIZE + i];
                    if (c >= 32 && c <= 126) printf("%c", c);
                    else printf(".");
                }
                printf("\"\n");
                
                ack.ack_no = expected_seq[st]; 
                expected_seq[st] = (expected_seq[st] + 1) % 2; 
            } else {
                printf("  [Station %d] Seq %d DISCARDED (Duplicate).\n", st, received_seq);
                ack.ack_no = (expected_seq[st] == 0) ? 1 : 0; 
            }
            
            
            if (!ack_sent) {
                sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, addr_len);
                ack_sent = 1;
            }
        }
        }
        printf("\n");
    }
    close(sockfd); return 0;
}
