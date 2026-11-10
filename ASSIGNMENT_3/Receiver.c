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

#define PORT 4456 
#define HEADER_SIZE 16
#define PAYLOAD_SIZE 46
#define MAX_BUFFER_SIZE 100

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

    int crc_choice; printf("======== CRC ========:\n1. CRC 8\nChoice: "); scanf("%d", &crc_choice);
    int length = HEADER_SIZE + PAYLOAD_SIZE + 2 + 1; 

    int arq_choice; printf("\n------- Flow Control -------\n1. Stop and Wait ARQ\nEnter expected protocol: "); scanf("%d", &arq_choice);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; server_addr.sin_port = htons(PORT); server_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    
    printf("Server listening on port %d...\n\n", PORT);
    unsigned short expected_seq = 0; 
    int senders_finished = 0;

    while (1) {
        int bytes_received = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        
        if (bytes_received == 4 && strncmp((char *)buffer, "DONE", 4) == 0) {
            senders_finished++;
            printf("\n[Server] Received DONE signal from a sender (%d/%d completed).\n", senders_finished, expected_senders);
            if (senders_finished >= expected_senders) {
                printf("[Server] All senders finished. Ending transmission...\n");
                break;
            }
            continue; // Skip the rest of the loop for this packet
        } 

        if(bytes_received == length) {
            FrameHeader *hdr = (FrameHeader *)buffer;
            unsigned short received_seq = hdr->seq_no;
            AckFrame ack;
            
            if (received_seq == expected_seq) {
                printf("○ Sequence %d accepted (In-Order).\n", received_seq);
                ack.ack_no = expected_seq; expected_seq = (expected_seq + 1) % 2; 
            } else {
                printf("○ Sequence %d DISCARDED (Duplicate).\n", received_seq);
                ack.ack_no = (expected_seq == 0) ? 1 : 0; 
            }
            sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, addr_len);
            printf("-> Sent ACK %d.\n", ack.ack_no);
        }
    }
    close(sockfd); return 0;
}