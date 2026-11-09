
unsigned short cal_chksum(unsigned short *frame, int length)
{
    unsigned int sum = 0;
    for(int i = 0; i < length / 2; i ++)
    {
        sum += frame[i];
    }

    if(length % 2 != 0)
        sum += ((unsigned char *)frame)[length - 1];

    while(sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (unsigned short)(~sum);
}