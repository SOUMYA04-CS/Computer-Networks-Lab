#include <stdio.h>
#include <stdint.h>

unsigned char cal_crc8(unsigned char *data, int length) {
    unsigned char crc = 0x00;
    unsigned char polynomial = 0xD5;

    for (int i = 0; i < length; i++) {
        crc ^= data[i]; 
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) { 
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

unsigned short cal_crc10(unsigned char *data, int length) {
    unsigned short crc = 0x0000;
    unsigned short polynomial = 0x0233;

    for (int i = 0; i < length; i++) {
        crc ^= (data[i] << 2); 
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0200) { 
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc & 0x03FF; 
}

unsigned short cal_crc16(unsigned char *data, int length) {
    unsigned short crc = 0x0000;
    unsigned short polynomial = 0x8005;

    for (int i = 0; i < length; i++) {
        crc ^= (data[i] << 8); 
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) { 
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

uint32_t cal_crc32(unsigned char *data, int length) {
    uint32_t crc = 0x00000000; 
    uint32_t polynomial = 0x04C11DB7;

    for (int i = 0; i < length; i++) {
        crc ^= ((uint32_t)data[i] << 24); 
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80000000) { 
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}