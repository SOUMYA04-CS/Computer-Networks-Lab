#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define CHANNEL_PORT 4455
#define RECEIVER_PORT 4456
#define TRANS_DELAY_MS 50 

typedef enum { IDLE, BUSY, COLLISION } ChannelState;

long long current_timestamp() {
    struct timeval te; gettimeofday(&te, NULL); 
    return te.tv_sec * 1000LL + te.tv_usec / 1000;
}

int main(int argc, char *argv[]) {
    if(argc != 2) { printf("Usage: %s <Receiver_IP>\n", argv[0]); return 1; }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in channel_addr, receiver_addr, client_addr;
    struct sockaddr_in last_sender; 
    unsigned char buffer[1024];

    memset(&channel_addr, 0, sizeof(channel_addr));
    channel_addr.sin_family = AF_INET;
    channel_addr.sin_port = htons(CHANNEL_PORT);
    channel_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sockfd, (struct sockaddr *)&channel_addr, sizeof(channel_addr));

    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(RECEIVER_PORT);
    receiver_addr.sin_addr.s_addr = inet_addr(argv[1]);

    ChannelState state = IDLE;
    long long busy_until = 0;
    int total_collisions = 0;

    printf("[Channel] Emulating shared medium on port %d...\n", CHANNEL_PORT);

    while(1) {
        socklen_t addr_len = sizeof(client_addr);
        int bytes = recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr *)&client_addr, &addr_len);
        
        long long now = current_timestamp();
        if (now >= busy_until && (state == BUSY || state == COLLISION)) state = IDLE; 

        if (bytes == 5 && strncmp((char *)buffer, "SENSE", 5) == 0) {
            char reply[10];
            if (state == IDLE) strcpy(reply, "IDLE");
            else if (state == BUSY) strcpy(reply, "BUSY");
            else strcpy(reply, "COLLISION");
            sendto(sockfd, reply, strlen(reply), 0, (struct sockaddr *)&client_addr, addr_len);
        } 
        else if (ntohs(client_addr.sin_port) == RECEIVER_PORT) {
            sendto(sockfd, buffer, bytes, 0, (struct sockaddr *)&last_sender, sizeof(last_sender));
        } 
        else { 
            if (state == IDLE) {
                state = BUSY;
                busy_until = now + TRANS_DELAY_MS;
                last_sender = client_addr; 
                sendto(sockfd, buffer, bytes, 0, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
            } else {
                state = COLLISION; 
                total_collisions++;
                printf("[Channel] COLLISION DETECTED! Total: %d\n", total_collisions);
                busy_until = now + TRANS_DELAY_MS; 
                buffer[16] ^= 0xFF;
                sendto(sockfd, buffer, bytes, 0, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
            }
        }
    }
    return 0;
}