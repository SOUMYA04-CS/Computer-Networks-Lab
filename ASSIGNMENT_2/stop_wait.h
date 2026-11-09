#ifndef STOP_WAIT_H
#define STOP_WAIT_H
#define PAYLOAD_SIZE 46
#define HEADER_SIZE 16
#define MAXIMUM_SIZE 100

void send_stop_and_wait(FILE *fp, struct sockaddr_in *server_addr, int sockfd, int crc_choice, int err_choice, float error_prob) {
    unsigned char payload[PAYLOAD_SIZE];
    int byte_read;
    unsigned char seq_no = 0;

    while((byte_read = fread(payload, 1, PAYLOAD_SIZE, fp)) > 0) {
        if(byte_read < PAYLOAD_SIZE)
            memset(payload + byte_read, 0, PAYLOAD_SIZE - byte_read);

        unsigned char frame[MAXIMUM_SIZE]; 
        memset(frame, 0, sizeof(frame));

        FrameHeader *hdr = (FrameHeader *)frame;
        hdr->length = htons(byte_read);
        hdr->seq_no = seq_no;

        memcpy(frame + HEADER_SIZE, payload, PAYLOAD_SIZE);
        int BASE_FRAME_LENGTH = HEADER_SIZE + PAYLOAD_SIZE;

        unsigned short checksum = cal_chksum((unsigned short *)frame, BASE_FRAME_LENGTH);
        frame[BASE_FRAME_LENGTH] = checksum & 0xFF;            
        frame[BASE_FRAME_LENGTH + 1] = (checksum >> 8) & 0xFF; 
        int TOTAL_FRAME_LENGTH = BASE_FRAME_LENGTH + 2;

        unsigned char crc8; unsigned short crc16; uint32_t crc32;
        switch(crc_choice) {
            case 8:
                crc8 = cal_crc8(frame, BASE_FRAME_LENGTH);
                frame[TOTAL_FRAME_LENGTH] = crc8;
                TOTAL_FRAME_LENGTH += 1; break;
            case 10:
                crc16 = cal_crc10(frame, BASE_FRAME_LENGTH);
                frame[TOTAL_FRAME_LENGTH] = crc16 & 0xFF; frame[TOTAL_FRAME_LENGTH + 1] = (crc16 >> 8) & 0xFF;
                TOTAL_FRAME_LENGTH += 2; break;
            case 16:
                crc16 = cal_crc16(frame, BASE_FRAME_LENGTH);
                frame[TOTAL_FRAME_LENGTH] = crc16 & 0xFF; frame[TOTAL_FRAME_LENGTH + 1] = (crc16 >> 8) & 0xFF;
                TOTAL_FRAME_LENGTH += 2; break;
            case 32: 
                crc32 = cal_crc32(frame, BASE_FRAME_LENGTH);
                frame[TOTAL_FRAME_LENGTH] = crc32 & 0xFF; frame[TOTAL_FRAME_LENGTH + 1] = (crc32 >> 8) & 0xFF;
                frame[TOTAL_FRAME_LENGTH + 2] = (crc32 >> 16) & 0xFF; frame[TOTAL_FRAME_LENGTH + 3] = (crc32 >> 24) & 0xFF;
                TOTAL_FRAME_LENGTH += 4; break;
        }

        int ack_received = 0;
        while (!ack_received) {
            if (Channel(error_prob)) {
                if (rand() % 2 == 0) {
                    printf("[Channel] Frame %d LOST in network.\n", seq_no);
                } else {
                    printf("[Channel] Frame %d CORRUPTED during transit.\n", seq_no);
                    unsigned char send_frame[MAXIMUM_SIZE];
                    memcpy(send_frame, frame, TOTAL_FRAME_LENGTH);
                    int current_frame_error = err_choice;
                    if (err_choice == 0 || err_choice == 1) current_frame_error = (rand() % 4) + 2; 
                    injkterror(send_frame, TOTAL_FRAME_LENGTH, current_frame_error);
                    sendto(sockfd, send_frame, TOTAL_FRAME_LENGTH, 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
                }
            } else {
                sendto(sockfd, frame, TOTAL_FRAME_LENGTH, 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
                printf("Sent Sequence %d to Receiver.\n", seq_no);
            }

            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sockfd, &readfds);
            struct timeval tv;
            tv.tv_sec = 2; tv.tv_usec = 0;

            int ret = select(sockfd + 1, &readfds, NULL, NULL, &tv);
            if (ret > 0 && FD_ISSET(sockfd, &readfds)) {
                AckFrame ack;
                socklen_t len = sizeof(*server_addr);
                int n = recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)server_addr, &len);
                if (n > 0 && ack.ack_no == seq_no) {
                    printf("-> Received ACK for Sequence %d.\n\n", seq_no);
                    ack_received = 1;
                    seq_no = (seq_no + 1) % 2; 
                }
            } else {
                printf("-> Timeout! Retransmitting Sequence %d...\n", seq_no);
            }
        }
    }
    char end[] = "DONE";
    sendto(sockfd, end, strlen(end), 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
    printf("File transfer complete. Sent DONE signal.\n");
}

#endif 