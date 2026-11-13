#ifndef WALSH_H
#define WALSH_H

#define MAX_WALSH_SIZE 64

int next_power_of_2(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return (p < 2) ? 2 : p;
}

void generateWalshTable(int N, int table[MAX_WALSH_SIZE][MAX_WALSH_SIZE]) {
    table[0][0] = 1;
    for (int len = 1; len < N; len <<= 1) {
        for (int i = 0; i < len; i++) {
            for (int j = 0; j < len; j++) {
                table[i][j + len]       =  table[i][j];
                table[i + len][j]       =  table[i][j];
                table[i + len][j + len] = -table[i][j];
            }
        }
    }
}
#endif
