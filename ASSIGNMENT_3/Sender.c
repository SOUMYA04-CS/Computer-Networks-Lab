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

#define PORT 4455
#define PAYLOAD_SIZE 46
#define HEADER_SIZE 16
#define MAXIMUM_SIZE 100

int csma_strategy = 1;
float csma_p_prob = 0.5;

long total_mac_delay_us = 0;
int total_frames_sent = 0;

// --- ANTI-DEADLOCK SENSE CHANNEL ---
int sense_channel(int sockfd, struct sockaddr_in *channel_addr) {
    char reply[10];
    socklen_t len = sizeof(*channel_addr);
    sendto(sockfd, "SENSE", 5, 0, (struct sockaddr *)channel_addr, sizeof(*channel_addr));
    
    // 100ms Timeout to prevent UDP freezing when senders launch simultaneously
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; 

    int ret = select(sockfd + 1, &readfds, NULL, NULL, &tv);
    if (ret > 0 && FD_ISSET(sockfd, &readfds)) {
        recvfrom(sockfd, reply, 10, 0, (struct sockaddr *)channel_addr, &len);
        if (strncmp(reply, "IDLE", 4) == 0) return 0;
        if (strncmp(reply, "BUSY", 4) == 0) return 1;
        return 2; 
    }
    // If packet drops, safely assume channel is busy
    return 1; 
}

void csma_sendto(int sockfd, const void *msg, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    if (len <= 5) { sendto(sockfd, msg, len, flags, dest_addr, addrlen); return; }
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    struct sockaddr_in *channel_addr = (struct sockaddr_in *)dest_addr;

    while(1) {
        int status = sense_channel(sockfd, channel_addr);
        if (status == 0) { 
            if (csma_strategy == 3) { 
                if (((float)rand() / (float)RAND_MAX) <= csma_p_prob) {
                    sendto(sockfd, msg, len, flags, dest_addr, addrlen); break;
                } else { usleep(50000); continue; }
            } else { 
                sendto(sockfd, msg, len, flags, dest_addr, addrlen); break;
            }
        } else { 
            if (csma_strategy == 1) usleep((rand() % 100) * 1000); 
            else usleep(5000); 
        }
    }
    gettimeofday(&end, NULL);
    long delay = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    total_mac_delay_us += delay;
    total_frames_sent++;
    printf("Frame transmitted via CSMA. MAC Delay: %ld microsec\n", delay);
}

#pragma pack(1)
typedef struct {
    unsigned char src_mac[6]; unsigned char dest_mac[6];
    unsigned short length; unsigned short seq_no; 
} FrameHeader;
typedef struct { unsigned short ack_no; } AckFrame;
#pragma pack()

void injkterror(unsigned char *frame, int length, int error_choice) {
    int rad_biter_1, rad_byter_1;
    if (error_choice == 2) {
        rad_byter_1 = rand() % length; rad_biter_1 = rand() % 8;
        frame[rad_byter_1] ^= (1 << rad_biter_1);
    }
}

int Channel(float error_prob) { return ((float)rand() / (float)RAND_MAX) < error_prob; }

#include "Crc.h"
#include "stop_wait.h"

int main(int argc, char *argv[]) {
    if(argc != 3) { printf("Usage: %s <Receiver_IP> <Test_File>\n", argv[0]); return 1; }

    srand(time(NULL) ^ getpid()); 
    int sockfd = socket(PF_INET, SOCK_DGRAM, 0); 
    struct sockaddr_in server_addr;
    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    FILE *fp = fopen(argv[2], "rb");
    if(fp == NULL) { printf("File not found\n"); return 1; }

    fseek(fp, 0, SEEK_END); long file_size_bytes = ftell(fp); fseek(fp, 0, SEEK_SET);

    int crc_choice, err_choice, arq_choice; float error_prob = 0.0;
    printf("Choose CRC (8, 10, 16, 32): "); scanf("%d", &crc_choice);
    printf("\n------- Error Injection Mode -------\n1. Random error\n2. Selected error\nChoice: ");
    int mode_choice; scanf("%d", &mode_choice); err_choice = (mode_choice == 2) ? 1 : 0; 
    printf("\n------- Flow Control Protocol -------\n1. Stop and Wait ARQ\nEnter choice: "); scanf("%d", &arq_choice);
    
    printf("\n------- CSMA Strategy -------\n1. Non-Persistent\n2. 1-Persistent\n3. p-Persistent\n4. CSMA/CD\nChoice: ");
    scanf("%d", &csma_strategy);
    if(csma_strategy == 3) { printf("Enter probability p (0.0 to 1.0): "); scanf("%f", &csma_p_prob); }

    struct timeval t_start, t_end; gettimeofday(&t_start, NULL);

    if(arq_choice == 1) send_stop_and_wait(fp, &server_addr, sockfd, crc_choice, err_choice, error_prob);

    gettimeofday(&t_end, NULL);
    double total_time_sec = (t_end.tv_sec - t_start.tv_sec) + ((t_end.tv_usec - t_start.tv_usec) / 1000000.0);
    double throughput = (file_size_bytes * 8.0) / total_time_sec; 
    double avg_mac_delay_ms = (total_mac_delay_us / 1000.0) / (double)total_frames_sent;

    printf("\n=== METRICS ===\n");
    printf("Total Time: %.2f seconds\n", total_time_sec);
    printf("Throughput: %.2f bps\n", throughput);
    printf("Avg MAC Delay: %.2f ms\n", avg_mac_delay_ms);
    printf("===============\n");
    fclose(fp); close(sockfd); return 0;
}