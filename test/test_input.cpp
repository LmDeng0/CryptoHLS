// A test file for our AnalysisEngine

void aes_accelerator(int key[4], int data_in[16]) {

    int internal_state[16]; // This is an array

    // This is the main loop
    for (int i = 0; i < 10; i++) {

        float temp_buffer[4]; // Another array

        // This is a nested loop
        for (int j = 0; j < 16; j++) {
            internal_state[j] = data_in[j] ^ key[j % 4];
        }
    }
}

int main() {
    int k[4], d[16];
    aes_accelerator(k, d);
    return 0;
}




