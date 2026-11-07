#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include "checksum.h"
#include "Crc.h"


#define PORT 4455
#define PAYLOAD_SIZE 46
#define HEADDER_SIZE 16
#define MAXIMUM_SIZE 68

void injkterror(unsigned char *frame, int length, int error_choice)
{
    int rad_biter_1, rad_byter_1, rad_biter_2, rad_byter_2;

    switch(error_choice)
    {
        case 1:
            printf("<[No error injected]>\n");
            break;

        case 2:
            rad_byter_1 = rand() % length;
            rad_biter_1 = rand() % 8;
            frame[rad_byter_1] ^= (1 << rad_biter_1);
            printf("-> [Single-Bit Error] Injected at byte %d, bit %d\n", rad_byter_1, rad_biter_1);
            break;

        case 3:
            rad_byter_1 = rand() % (length / 2);
            rad_byter_2 = (length / 2) + (rand() % (length / 2));
            rad_biter_1 = rand() % 8;
            rad_biter_2 = rand() % 8;
            frame[rad_byter_1] ^= (1 << rad_biter_1);
            frame[rad_byter_2] ^= (1 << rad_biter_2);
            printf("-> [Two Isolated Errors] Injected at byte %d (bit %d) and byte %d (bit %d)\n", rad_byter_1, rad_biter_1, rad_byter_2, rad_biter_2);
            break;

        case 4: 
            printf("-> [Odd Number of Errors] Injected 3 errors:\n");
            for(int i = 0; i < 3; i++) {
                rad_byter_1 = rand() % length;
                rad_biter_1 = rand() % 8;
                frame[rad_byter_1] ^= (1 << rad_biter_1);
                printf("   - Byte %d, bit %d\n", rad_byter_1, rad_biter_1);
            }
            break;

        case 5: 
            rad_byter_1 = rand() % length;
            frame[rad_byter_1] ^= 0x1E;
            printf("-> [Burst Error] Injected 4 consecutive flipped bits at byte %d\n", rad_byter_1);
            break;

        default:
            printf("Wrong choice. No error injected\n");
            break;
    }
}

void send_frames(FILE *fp,struct sockaddr_in *server_addr, int sockfd, int crc_choice, int err_choice)
{
    unsigned char payload[PAYLOAD_SIZE];
    int byte_read;
    int BASE_FRAME_LENGTH = HEADDER_SIZE + PAYLOAD_SIZE;
    unsigned char crc;

    while((byte_read = fread(payload, 1, PAYLOAD_SIZE, fp)) > 0)
    {
        if(byte_read < PAYLOAD_SIZE)
            memset(payload + byte_read, 0, PAYLOAD_SIZE - byte_read);

        unsigned char frame[70]; 
        memset(frame, 0, sizeof(frame));
        memcpy(frame + HEADDER_SIZE, payload, PAYLOAD_SIZE);

        memset(frame, 0, HEADDER_SIZE);
        memcpy(frame + HEADDER_SIZE, payload, PAYLOAD_SIZE);

        unsigned short checksum = cal_chksum((unsigned short *)frame, BASE_FRAME_LENGTH);
        frame[BASE_FRAME_LENGTH] = checksum & 0xFF;            
        frame[BASE_FRAME_LENGTH + 1] = (checksum >> 8) & 0xFF; 

        int TOTAL_FRAME_LENGTH = BASE_FRAME_LENGTH + 2;

        switch(crc_choice)
        {
            case 8:
                crc = cal_crc8(frame, BASE_FRAME_LENGTH);
                frame[TOTAL_FRAME_LENGTH] = crc;
                TOTAL_FRAME_LENGTH += 1;
                break;
            
            case 10:
                crc = cal_crc10(frame, BASE_FRAME_LENGTH);
                frame[TOTAL_FRAME_LENGTH] = crc & 0xFF;
                frame[TOTAL_FRAME_LENGTH + 1] = (crc >> 8) & 0xFF;
                TOTAL_FRAME_LENGTH += 2;
                break;

            case 16:
                crc = cal_crc16(frame, BASE_FRAME_LENGTH);
                frame[TOTAL_FRAME_LENGTH] = crc & 0xFF;
                frame[TOTAL_FRAME_LENGTH + 1] = (crc >> 8) & 0xFF;
                TOTAL_FRAME_LENGTH += 2;
                break;

            case 32: 
                crc = cal_crc32(frame, BASE_FRAME_LENGTH);
                frame[TOTAL_FRAME_LENGTH] = crc & 0xFF;
                frame[TOTAL_FRAME_LENGTH + 1] = (crc >> 8) & 0xFF;
                frame[TOTAL_FRAME_LENGTH + 2] = (crc >> 16) & 0xFF;
                frame[TOTAL_FRAME_LENGTH + 3] = (crc >> 24) & 0xFF;
                TOTAL_FRAME_LENGTH += 4;
                break;
        }
        int current_frame_error = err_choice;
        
        if (err_choice == 0) current_frame_error = (rand() % 5) + 1; 

        injkterror(frame, TOTAL_FRAME_LENGTH, current_frame_error);

        sendto(sockfd, frame, TOTAL_FRAME_LENGTH, 0, (struct sockaddr *)server_addr, sizeof(*server_addr)); // sends tha data
        printf("Sent %d bytes (Total Frame Length) to Receiver.\n", TOTAL_FRAME_LENGTH);

        usleep(50000);
    }

    char end[] = "DONE";
    sendto(sockfd, end, strlen(end), 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
    printf("File transfer complete. Sent DONE signal to Server.\n");
}

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        printf("Usage: %s <Receiver_IP> <Test_File>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));
    int sockfd;
    struct sockaddr_in server_addr;
    FILE *fp = fopen(argv[2], "rb");
    if(fp == NULL)            
    {
        printf("File not found\n");
        return 1;
    }

    sockfd = socket(PF_INET, SOCK_DGRAM, 0); 
    memset(&server_addr , '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    int crc_choice, err_choice, mode_choice;
    printf("Choose expected choise:\n");
    printf("1. CRC 8\n");
    printf("2. CRC 10\n");
    printf("3. CRC 16\n");
    printf("4. CRC 32\n");
    scanf("%d", &crc_choice);
    if (crc_choice != 8 && crc_choice != 10 && crc_choice != 16 && crc_choice != 32) {
        printf("Invalid CRC choice. Defaulting to 16.\n");
        crc_choice = 16;
    }

    printf("\n------- Error Injection Mode -------\n");
    printf("1. Random error (Random type per frame)\n");
    printf("2. Selected error (Choose a specific error type for all frames)\n");
    printf("Enter choice: ");
    scanf("%d", &mode_choice);
    if (mode_choice == 2) {
        printf("\n------- Select Error Type -------\n");
        printf("1. No error\n");
        printf("2. Single bit error\n");
        printf("3. Two Isolated Single-bit Errors\n");
        printf("4. Odd Number of Errors (3 bits)\n");
        printf("5. Burst Error\n");
        printf("Enter choice: ");
        scanf("%d", &err_choice);
    } else {
        err_choice = 0; 
    }

    send_frames(fp, &server_addr, sockfd, crc_choice, err_choice);

    fclose(fp);
    close(sockfd);
    return 0;
}