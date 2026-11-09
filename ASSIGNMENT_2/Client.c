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
#include "checksum.h"


#define PORT 4455
#define PAYLOAD_SIZE 46
#define HEADER_SIZE 16
#define MAXIMUM_SIZE 100

#pragma pack(1)
typedef struct {
    unsigned char src_mac[6];
    unsigned char dest_mac[6];
    unsigned short length;
    unsigned short seq_no; // <--- UPGRADED to 16-bit (2 bytes)
    // (padding removed because seq_no now takes up that extra byte)
} FrameHeader;

typedef struct {
    unsigned short ack_no; // <--- UPGRADED to 16-bit 
} AckFrame;
#pragma pack()

void injkterror(unsigned char *frame, int length, int error_choice) {
    int rad_biter_1, rad_byter_1, rad_biter_2, rad_byter_2;
    switch(error_choice) {
        case 1: break; 
        case 2:
            rad_byter_1 = rand() % length;
            rad_biter_1 = rand() % 8;
            frame[rad_byter_1] ^= (1 << rad_biter_1);
            break;
        case 3:
            rad_byter_1 = rand() % (length / 2);
            rad_byter_2 = (length / 2) + (rand() % (length / 2));
            rad_biter_1 = rand() % 8;
            rad_biter_2 = rand() % 8;
            frame[rad_byter_1] ^= (1 << rad_biter_1);
            frame[rad_byter_2] ^= (1 << rad_biter_2);
            break;
        case 4: 
            for(int i = 0; i < 3; i++) {
                rad_byter_1 = rand() % length;
                rad_biter_1 = rand() % 8;
                frame[rad_byter_1] ^= (1 << rad_biter_1);
            }
            break;
        case 5: 
            rad_byter_1 = rand() % length;
            frame[rad_byter_1] ^= 0x1E;
            break;
    }
}

int Channel(float error_prob) {
    float random_val = (float)rand() / (float)RAND_MAX;
    if (random_val < error_prob) return 1; 
    return 0; 
}

#include "Crc.h"
#include "stop_wait.h"
#include "go_back_n.h"
#include "selective_repeat.h"

int main(int argc, char *argv[]) {
    if(argc != 3) {
        printf("Usage: %s <Receiver_IP> <Test_File>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));
    int sockfd = socket(PF_INET, SOCK_DGRAM, 0); 
    struct sockaddr_in server_addr;
    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    FILE *fp = fopen(argv[2], "rb");
    if(fp == NULL) {
        printf("File not found\n");
        return 1;
    }

    int crc_choice, err_choice, arq_choice, window_size = 1;
    float error_prob;

    printf("Choose CRC (8, 10, 16, 32): ");
    scanf("%d", &crc_choice);
    if (crc_choice != 8 && crc_choice != 10 && crc_choice != 16 && crc_choice != 32) crc_choice = 16;

    printf("\n------- Error Injection Mode -------\n");
    printf("1. Random error\n2. Selected error\nChoice: ");
    int mode_choice; scanf("%d", &mode_choice);
    if (mode_choice == 2) {
        printf("1. No error\n2. Single bit\n3. Double bit\n4. Odd errors\n5. Burst\nChoice: ");
        scanf("%d", &err_choice);
    } else err_choice = 0; 

    printf("\n------- Flow Control Protocol -------\n");
    printf("1. Stop and Wait ARQ\n");
    printf("2. Go-Back-N ARQ\n");
    printf("3. Selective Repeat ARQ\n");
    printf("Enter choice: ");
    scanf("%d", &arq_choice);

    if(arq_choice == 2 || arq_choice == 3){
        printf("Enter Window Size (N): ");
        scanf("%d", &window_size);
    }

    printf("Enter network error/loss probability (e.g., 0.2 for 20%%): ");
    scanf("%f", &error_prob);
    printf("\n");

    switch(arq_choice){
        case 1:
            send_stop_and_wait(fp, &server_addr, sockfd, crc_choice, err_choice, error_prob);
            break;
        case 2: 
            send_go_back_n(fp, &server_addr, sockfd, crc_choice, err_choice, error_prob, window_size);
            break;
        case 3:
            send_selective_repeat(fp, &server_addr, sockfd, crc_choice, err_choice, error_prob, window_size);
            break;
        default:
            printf("Wrong choice\n");
            break;
    }

    fclose(fp);
    close(sockfd);
    return 0;
}