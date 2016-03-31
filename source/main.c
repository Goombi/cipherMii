#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <3ds.h>
#define MII_ENCSIZE 0x70
#define MII_DECSIZE 0x60
typedef u8 Mii[0x70];

Handle aptuHandle;

Result APT_Unwrap(u32 outputSize, u32 inputSize, u32 blockSize, u32 nonceSize, void* input, void* output) {
	u32* cmdbuf=getThreadCommandBuffer();
	cmdbuf[0]=IPC_MakeHeader(0x47,4,4); // 0x470104
	cmdbuf[1]=outputSize;
	cmdbuf[2]=inputSize;
	cmdbuf[3]=blockSize;
	cmdbuf[4]=nonceSize;
	cmdbuf[5]=(inputSize << 4) | 0xA;
	cmdbuf[6]=(u32) input;
	cmdbuf[7]=(outputSize << 4) | 0xC;
	cmdbuf[8]=(u32) output;

	Result ret=0;
	if(R_FAILED(ret=svcSendSyncRequest(aptuHandle)))return ret;

	return cmdbuf[1];
}

Result APT_Wrap(u32 outputSize, u32 inputSize, u32 blockSize, u32 nonceSize, void* input, void* output) {
	u32* cmdbuf=getThreadCommandBuffer();
	cmdbuf[0]=IPC_MakeHeader(0x46,4,4); // 0x470104
	cmdbuf[1]=outputSize;
	cmdbuf[2]=inputSize;
	cmdbuf[3]=blockSize;
	cmdbuf[4]=nonceSize;
	cmdbuf[5]=(inputSize << 4) | 0xA;
	cmdbuf[6]=(u32) input;
	cmdbuf[7]=(outputSize << 4) | 0xC;
	cmdbuf[8]=(u32) output;

	Result ret=0;
	if(R_FAILED(ret=svcSendSyncRequest(aptuHandle)))return ret;

	return cmdbuf[1];
}

/**
 * Decrypt a Mii (at least tries to)
 * @param char* inputFile  Input filename, encrypted Mii
 * @param char* outputFile Output filename, decrypted Mii
 * @return int Return code: 0 means SUCCESS. 1 is APT:Unwrap failure.
 *             2 is wrong input. 126 is IO error. 127 is memory allocation.
 */
int decryptMii(const char* inputFile, const char* outputFile) {
	// Open inputFile
	FILE* fd = fopen(inputFile, "r");
	if (!fd) {
		printf("ERROR: Couldn't open %s.\n", inputFile);
		return 126;
	}
	// Read inputFile
	Mii input;
	u32 size = fread(&input, 1, MII_ENCSIZE, fd);
	fclose(fd);
	if (size != MII_ENCSIZE) {
		printf("ERROR: %s doesn't have the expected size.\n", inputFile);
		return 2;
	}
	// Call APT:Unwrap
	Mii output;
	if(!R_SUCCEEDED(APT_Unwrap(MII_DECSIZE, MII_ENCSIZE, 12, 10, &input, &output))) {
		printf("ERROR: Failed to decrypt %s.\n", inputFile);
		return 1;
	}
	// Open ouputFile
	fd = fopen(outputFile, "w");
	if (!fd) {
		printf("ERROR: Couldn't write to %s.\n", outputFile);
		return 126;
	}
	// Write to outputFile
	size = fwrite(&output, 1, MII_DECSIZE, fd);
	fclose(fd);
	printf("SUCCESS: %s decrypted to %s.\n", inputFile, outputFile);
	return 0;
}

/**
 * Encrypt a Mii (hopefully)
 * @param char* inputFile  Input filename, decrypted Mii
 * @param char* outputFile Output filename, encrypted Mii
 * @return int Return code: 0 means SUCCESS. 1 is APT:Wrap failure.
 *             2 is wrong input. 126 is IO error.
 */
int encryptMii(const char* inputFile, const char* outputFile) {
	// Open inputFile
	FILE* fd = fopen(inputFile, "r");
	if (!fd) {
		printf("ERROR: Couldn't open %s.\n", inputFile);
		return 126;
	}
	// Read inputFile
	Mii input;
	u32 size = fread(&input, 1, MII_DECSIZE, fd);
	fclose(fd);
	if (size != MII_DECSIZE) {
		printf("ERROR: %s doesn't have the expected size.\n", inputFile);
		return 2;
	}
	// Call APT:Unwrap
	Mii output;
	if(!R_SUCCEEDED(APT_Wrap(MII_ENCSIZE, MII_DECSIZE, 12, 10, &input, &output))) {
		printf("ERROR: Failed to encrypt %s.\n", inputFile);
		return 1;
	}
	// Open ouputFile
	fd = fopen(outputFile, "w");
	if (!fd) {
		printf("ERROR: Couldn't write to %s.\n", outputFile);
		return 126;
	}
	// Write to outputFile
	size = fwrite(&output, 1, MII_ENCSIZE, fd);
	fclose(fd);
	printf("SUCCESS: %s encrypted to %s.\n", inputFile, outputFile);
	return 0;
}

/**
 * Compare two files
 * @param char* file1 First of the two files.
 * @param char* file2 The other file.
 * @param u32 size Expected size of the files.
 * @return int Return code. 0 means Success. 1 is different files.
 *             2 is wrong input. 126 is IO error.
 */
int compareFiles(const char* file1, const char* file2, u32 size) {
    // Open file 1
    FILE* fd = fopen(file1, "r");
    if (!fd) {
        printf("ERROR: Couldn't open %s.\n", file1);
        return 126;
    }
    Mii data1;
    u32 read = fread(&data1, 1, size, fd);
    if (read != size) {
        printf("ERROR: %s doesn't have the expected size.\n", file1);
        fclose(fd);
        return 2;
    }
    fclose(fd);
    // Open file 2
    fd = fopen(file2, "r");
    if (!fd) {
        printf("ERROR: Couldn't open %s.\n", file2);
        return 126;
    }
    Mii data2;
    read = fread(&data2, 1, size, fd);
    if (read != size) {
        printf("ERROR: %s doesn't have the expected size.\n", file2);
        fclose(fd);
        return 2;
    }
    fclose(fd);
    // Compare file
    for (u32 i = 0; i < size; i++) {
        if (data1[i] != data2[i]) {
            printf("ERROR: Files are different.");
            return 1;
        }
    }
    printf("SUCCESS: Same files.");
    return 0;
}

int main()
{
	// Initializations
	gfxInitDefault(); // graphics
	srvGetServiceHandle(&aptuHandle, "APT:U");
	consoleInit(GFX_BOTTOM, NULL);
	u32 kDown;        // keys down

	// Wait for next frame
	gspWaitForVBlank();
	printf("          === DecryptMii ===\n");
	printf("A : input.bin  -> output.mii\n");
	printf("B : input.mii  -> output.bin\n");
	printf("X : output.bin -> check.mii\n");
	printf("Y : output.mii -> check.bin\n");
	printf("L : Compare input.bin -> check.mii\n");
	printf("R : Compare input.mii -> check.bin\n");
	printf("Press Start to exit\n");
	while(aptMainLoop()) {
		gspWaitForVBlank();
		hidScanInput();
		kDown = hidKeysDown();
		if (kDown & KEY_START) break;
		if (kDown & KEY_A) {
			decryptMii("input.bin", "output.mii");
		}
		if (kDown & KEY_B) {
			encryptMii("input.mii", "output.bin");
		}
		if (kDown & KEY_X) {
			decryptMii("output.bin", "check.mii");
		}
		if (kDown & KEY_Y) {
			encryptMii("output.mii", "check.bin");
		}
		if (kDown & KEY_L) {
            decryptMii("input.bin", "test.mii");
			compareFiles("test.mii", "check.mii", MII_DECSIZE);
            unlink("test.mii");
		}
		if (kDown & KEY_R) {
            encryptMii("input.mii", "test.bin");
			compareFiles("test.bin", "check.bin", MII_ENCSIZE);
            unlink("test.bin");
		}
	}
	svcCloseHandle(aptuHandle);
	gfxExit();
	return 0;
}
