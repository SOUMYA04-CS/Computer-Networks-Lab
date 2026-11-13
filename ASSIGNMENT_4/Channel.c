#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/select.h>
#include "walsh.h"

#define CHANNEL_PORT 4455
#define RECEIVER_PORT 4456
#define MAX_CHIPS 32000 // Safely fits inside UDP MTU (~65KB)

int main(int argc, char *argv[]) {
    if(argc != 3) { printf("Usage: %s <Receiver_IP> <Num_Senders>\n", argv[0]); return 1; }

    int expected_senders = atoi(argv[2]);
    int walsh_N = next_power_of_2(expected_senders);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in channel_addr, receiver_addr, client_addr;
    
    struct sockaddr_in senders[64];
    int num_known_senders = 0;

    memset(&channel_addr, 0, sizeof(channel_addr));
    channel_addr.sin_family = AF_INET;
    channel_addr.sin_port = htons(CHANNEL_PORT);
    channel_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sockfd, (struct sockaddr *)&channel_addr, sizeof(channel_addr));

    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(RECEIVER_PORT);
    receiver_addr.sin_addr.s_addr = inet_addr(argv[1]);

    printf("[Channel] CDMA Multiplexer started on port %d.\n", CHANNEL_PORT);
    printf("[Channel] Expecting %d senders (Walsh N=%d).\n", expected_senders, walsh_N);

    int16_t composite[MAX_CHIPS];
    int current_chip_count = 0;
    int active_senders = expected_senders;

    while(active_senders > 0) {
        memset(composite, 0, sizeof(composite));
        int received_in_slot = 0;
        current_chip_count = 0;

        while (received_in_slot < active_senders) {
            unsigned char buffer[65000];
            socklen_t addr_len = sizeof(client_addr);
            
            
            fd_set readfds; FD_ZERO(&readfds); FD_SET(sockfd, &readfds);
            struct timeval tv; tv.tv_sec = 2; tv.tv_usec = 0;
            
            int ret = select(sockfd + 1, &readfds, NULL, NULL, &tv);
            if (ret <= 0) {
               if (received_in_slot == 0) {
                    continue; 
                } else {
                    printf("[Channel] Slot Timeout! Forwarding partial composite.\n");
                    break;   
                }
            }

            int bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &addr_len);
            
            
            if (ntohs(client_addr.sin_port) == RECEIVER_PORT) {
                for(int i=0; i<num_known_senders; i++) {
                    sendto(sockfd, buffer, bytes, 0, (struct sockaddr *)&senders[i], sizeof(senders[i]));
                }
                continue; 
            }

            
            int known = 0;
            for(int i=0; i<num_known_senders; i++) {
                if(senders[i].sin_port == client_addr.sin_port && senders[i].sin_addr.s_addr == client_addr.sin_addr.s_addr) { known = 1; break; }
            }
            if(!known && num_known_senders < 64) senders[num_known_senders++] = client_addr;

           
            if (bytes <= 10) {
                if (strncmp((char *)buffer, "DONE", 4) == 0) {
                    active_senders--;
                    printf("[Channel] Received DONE signal. %d active senders remaining.\n", active_senders);
                    sendto(sockfd, buffer, bytes, 0, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
                }
                received_in_slot++;
                continue;
            }

            
            int16_t *chips = (int16_t *)buffer;
            int num_chips = bytes / sizeof(int16_t);
            if (current_chip_count == 0) current_chip_count = num_chips;

            for(int i=0; i<num_chips && i<MAX_CHIPS; i++) composite[i] += chips[i];
            received_in_slot++;
        }

        if (current_chip_count > 0) {
            sendto(sockfd, composite, current_chip_count * sizeof(int16_t), 0, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
            printf("[Channel] Superimposed signals from %d senders -> Forwarded %d chips to Receiver.\n", received_in_slot, current_chip_count);
        }
    }
    return 0;
}