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
#define BUFFER_SIZE 64
#define MAX_BUFFER_SIZE 100

int main()
{
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    unsigned char buffer[MAX_BUFFER_SIZE];

    int crc_choice;
    printf("======== CRC ========:\n");
    printf("1. CRC 8\n");
    printf("2. CRC 10\n");
    printf("3. CRC 16\n");
    printf("4. CRC 32\n");
    scanf("%d", &crc_choice);
    if (crc_choice != 8 && crc_choice != 10 && crc_choice != 16 && crc_choice != 32) {
        printf("Invalid CRC choice. Defaulting to 16.\n");
        crc_choice = 16;
    }

    int length = BUFFER_SIZE;

    switch(crc_choice)
    {
        case 8:
            length += 1;
            break;

        case 10:
        case 16:
            length += 2;
            break;

        case 32:
            length += 4;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(sockfd);
        return 1;
    }
    printf("Server is listening on port %d...\n", PORT);

    int total_frames = 0;
    int both_detected = 0;
    int count_crc_error = 0;
    int count_chksum_error = 0;
    int missed_both = 0;

    while (1)
    {
        int bytes_received = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        
        if (bytes_received == 4 && strncmp((char *)buffer, "DONE", 4) == 0) 
        {
            printf("\n[Server] Received DONE signal. Ending transmission...\n");
            break; 
        }

        if(bytes_received == length)
        {
            unsigned short chk_result = cal_chksum((unsigned short *)buffer, BUFFER_SIZE);
            int chk_error = (chk_result != 0x0000);
            total_frames++;
            int crc_error = 0;
            uint32_t dis_calc_crc = 0;
            uint32_t dis_recv_crc = 0;

            switch(crc_choice)
            {
                case 8:
                    dis_calc_crc = cal_crc8(buffer, 62);
                    dis_recv_crc = buffer[64]; 
                    crc_error = (dis_calc_crc != dis_recv_crc);
                    break;

                case 10:
                    dis_calc_crc = cal_crc10(buffer, 62);
                    dis_recv_crc = buffer[64] | (buffer[65] << 8); 
                    crc_error = (dis_calc_crc != dis_recv_crc);
                    break;

                case 16:
                    dis_calc_crc = cal_crc16(buffer, 62);
                    dis_recv_crc = buffer[64] | (buffer[65] << 8); 
                    crc_error = (dis_calc_crc != dis_recv_crc);
                    break;

                case 32:
                    dis_calc_crc = cal_crc32(buffer, 62);
                    dis_recv_crc = (uint32_t)buffer[64] | ((uint32_t)buffer[65] << 8) | ((uint32_t)buffer[66] << 16) | ((uint32_t)buffer[67] << 24); 
                    crc_error = (dis_calc_crc != dis_recv_crc);
                    break;
            }

            if (chk_error || crc_error) {
                if (chk_error && crc_error) {
                    // printf("▲ Error is detected by both CRC and Checksum \n");
                    both_detected++;
                } else if (chk_error && !crc_error) {
                    // printf("⧎ Error is detected by checksum but not by CRC.\n");
                    count_chksum_error ++;
                } else if (!chk_error && crc_error) {
                    // printf("▮ Error is detected by CRC but not by Checksum.\n");
                    count_crc_error++;
                }else missed_both++;

                // printf("   [Details]\n");
                // if (chk_error) printf("   -> Checksum failed! Result evaluated to 0x%04X (Expected: 0x0000)\n", chk_result);

                // if (crc_error) printf("   -> CRC failed! Calculated: 0x%X | Received: 0x%X\n", dis_calc_crc, dis_recv_crc);
    
                // printf("   -> Frame Hex Dump: ");
                // for (int i = 0; i < bytes_received; i++) {
                //     if (i > 0 && i % 16 == 0) printf("\n                      "); // formatting
                //     printf("%02X ", buffer[i]);
                // }
                printf("[REJECTED] Error detected in frame!\n");
            
            } else printf("○ Frame accepted successfully. No errors detected.\n");
            printf("\n--------------------------------------------------\n");
            
        } else if (bytes_received > 0)printf("[ERROR] Received malformed frame of size %d bytes.\n", bytes_received);
    }

    printf("\n--- Final Results ---\n");
    printf("Total Frames Evaluated: %d\n", total_frames);
    printf("Both Detected: %d (%.2f%%)\n", both_detected, (float)both_detected/total_frames * 100);
    printf("Checksum Only: %d (%.2f%%)\n", count_chksum_error ++, (float)count_chksum_error ++/total_frames * 100);
    printf("CRC Only: %d (%.2f%%)\n", count_crc_error++, (float)count_crc_error++/total_frames * 100);
    printf("Missed by Both: %d (%.2f%%)\n", missed_both, (float)missed_both/total_frames * 100);

    close(sockfd);
    return 0;
}