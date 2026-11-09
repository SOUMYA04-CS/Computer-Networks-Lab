#ifndef GO_BACK_N_H
#define GO_BACK_N_H
#define PAYLOAD_SIZE 46
#define HEADER_SIZE 16
#define MAXIMUM_SIZE 100

void send_go_back_n(FILE *fp, struct sockaddr_in *server_addr, int sockfd, int crc_choice, int err_choice, float error_prob, int window_size) {
    unsigned char payload[PAYLOAD_SIZE];
    int byte_read;
    unsigned char frame_buffer[1000][MAXIMUM_SIZE];
    int frame_lengths[1000];
    int total_frames = 0,timeout_count = 0;

    while((byte_read = fread(payload, 1, PAYLOAD_SIZE, fp)) > 0 && total_frames < 1000) {
        if(byte_read < PAYLOAD_SIZE) memset(payload + byte_read, 0, PAYLOAD_SIZE - byte_read);
        unsigned char frame[MAXIMUM_SIZE]; 
        memset(frame, 0, sizeof(frame));

        FrameHeader *hdr = (FrameHeader *)frame;
        hdr->length = htons(byte_read);
        hdr->seq_no = total_frames; 
        
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
            case 16:
                crc16 = (crc_choice == 10) ? cal_crc10(frame, BASE_FRAME_LENGTH) : cal_crc16(frame, BASE_FRAME_LENGTH);
                frame[TOTAL_FRAME_LENGTH] = crc16 & 0xFF;
                frame[TOTAL_FRAME_LENGTH + 1] = (crc16 >> 8) & 0xFF;
                TOTAL_FRAME_LENGTH += 2; break;
            case 32: 
                crc32 = cal_crc32(frame, BASE_FRAME_LENGTH);
                frame[TOTAL_FRAME_LENGTH] = crc32 & 0xFF; frame[TOTAL_FRAME_LENGTH + 1] = (crc32 >> 8) & 0xFF;
                frame[TOTAL_FRAME_LENGTH + 2] = (crc32 >> 16) & 0xFF; frame[TOTAL_FRAME_LENGTH + 3] = (crc32 >> 24) & 0xFF;
                TOTAL_FRAME_LENGTH += 4; break;
        }
        memcpy(frame_buffer[total_frames], frame, TOTAL_FRAME_LENGTH);
        frame_lengths[total_frames] = TOTAL_FRAME_LENGTH;
        total_frames++;
    }

    int base = 0;
    int next_seq_num = 0;

    while (base < total_frames) {
        while (next_seq_num < base + window_size && next_seq_num < total_frames) {
            if (Channel(error_prob)) {
                if (rand() % 2 == 0) {
                    printf("[Channel] Frame %d LOST in network.\n", next_seq_num);
                } else {
                    printf("[Channel] Frame %d CORRUPTED.\n", next_seq_num);
                    unsigned char send_frame[MAXIMUM_SIZE];
                    memcpy(send_frame, frame_buffer[next_seq_num], frame_lengths[next_seq_num]);
                    int current_error = (err_choice == 0 || err_choice == 1) ? (rand() % 4) + 2 : err_choice;
                    injkterror(send_frame, frame_lengths[next_seq_num], current_error);
                    sendto(sockfd, send_frame, frame_lengths[next_seq_num], 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
                }
            } else {
                sendto(sockfd, frame_buffer[next_seq_num], frame_lengths[next_seq_num], 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
                printf("Sent Sequence %d.\n", next_seq_num);
            }
            next_seq_num++;
        }

        int timeout_occurred = 0;
        while (base < next_seq_num && !timeout_occurred) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sockfd, &readfds);
            struct timeval tv;
            tv.tv_sec = 2; tv.tv_usec = 0;

            int ret = select(sockfd + 1, &readfds, NULL, NULL, &tv);
            if (ret > 0 && FD_ISSET(sockfd, &readfds)) {
                AckFrame ack;
                socklen_t len = sizeof(*server_addr);
                recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)server_addr, &len);
                printf("-> Received Cumulative ACK %d.\n", ack.ack_no);
                if (ack.ack_no >= base && ack.ack_no < total_frames) {
                    base = ack.ack_no + 1; 
                }
            } else {
                printf("\n-> TIMEOUT! Retransmitting window from Sequence %d...\n\n", base);
                next_seq_num = base; 
                timeout_occurred = 1; 
                timeout_count++;
            }
        }
    }
    printf("\n==== PERFORMANCE METRICS ====\n");
    printf("Total Timeouts: %d\n", timeout_count);
    printf("=============================\n\n");

    char end[] = "DONE";
    sendto(sockfd, end, strlen(end), 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
    printf("File transfer complete. Sent DONE signal.\n");
}

#endif