#ifndef STOP_WAIT_H
#define STOP_WAIT_H
#define PAYLOAD_SIZE 46
#define HEADER_SIZE 16
#define MAXIMUM_SIZE 100

extern void csma_sendto(int sockfd, const void *msg, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

void send_stop_and_wait(FILE *fp, struct sockaddr_in *server_addr, int sockfd, int crc_choice, int err_choice, float error_prob) {
    unsigned char payload[PAYLOAD_SIZE];
    int byte_read;
    unsigned char seq_no = 0;

    while((byte_read = fread(payload, 1, PAYLOAD_SIZE, fp)) > 0) {
        if(byte_read < PAYLOAD_SIZE) memset(payload + byte_read, 0, PAYLOAD_SIZE - byte_read);

        unsigned char frame[MAXIMUM_SIZE]; memset(frame, 0, sizeof(frame));
        FrameHeader *hdr = (FrameHeader *)frame;
        hdr->length = htons(byte_read); hdr->seq_no = seq_no;
        memcpy(frame + HEADER_SIZE, payload, PAYLOAD_SIZE);
        int BASE_FRAME_LENGTH = HEADER_SIZE + PAYLOAD_SIZE;

        unsigned short checksum = cal_chksum((unsigned short *)frame, BASE_FRAME_LENGTH);
        frame[BASE_FRAME_LENGTH] = checksum & 0xFF;            
        frame[BASE_FRAME_LENGTH + 1] = (checksum >> 8) & 0xFF; 
        int TOTAL_FRAME_LENGTH = BASE_FRAME_LENGTH + 2;

        unsigned char crc8; 
        if (crc_choice == 8) { crc8 = cal_crc8(frame, BASE_FRAME_LENGTH); frame[TOTAL_FRAME_LENGTH] = crc8; TOTAL_FRAME_LENGTH += 1; }

        int ack_received = 0;
        while (!ack_received) {
            csma_sendto(sockfd, frame, TOTAL_FRAME_LENGTH, 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
            printf("Sent Sequence %d to Receiver.\n", seq_no);

        int waited_ms = 0;
        while (waited_ms < 400) {
            fd_set readfds; FD_ZERO(&readfds); FD_SET(sockfd, &readfds);
            struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 50000; // 50ms chunks

            int ret = select(sockfd + 1, &readfds, NULL, NULL, &tv);
            if (ret > 0 && FD_ISSET(sockfd, &readfds)) {
                unsigned char temp[128];
                socklen_t len = sizeof(*server_addr);
                int n = recvfrom(sockfd, temp, sizeof(temp), 0, (struct sockaddr *)server_addr, &len);
                
                // Only process it if it is exactly the size of an ACK frame (2 bytes)
                if (n == sizeof(AckFrame)) {
                    AckFrame *ack = (AckFrame *)temp;
                    if (ack->ack_no == seq_no) {
                        printf("-> Received ACK for Sequence %d.\n\n", seq_no);
                        ack_received = 1; 
                        seq_no = (seq_no + 1) % 2; 
                        break; 
                    }
                }
            }
            waited_ms += 50;
        }
        
        if (!ack_received) {
            printf("-> Timeout! Retransmitting Sequence %d...\n", seq_no);
        }
        }
    }
    char end[] = "DONE";
    sendto(sockfd, end, strlen(end), 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
    printf("File transfer complete. Sent DONE signal.\n");
}
#endif