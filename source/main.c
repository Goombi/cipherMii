#include <stdlib.h>
#include <stdio.h>
#include <3ds.h>
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
	if (fd == NULL) {
		printf("ERROR: Couldn't open %s.\n", inputFile);
		return 126;
	}
	// Allocate input buffer
	Mii* input = malloc(sizeof(Mii));
	if (input == NULL) {
		printf("ERROR: Memory allocation failed.\n");
		return 127;
	}
	// Read inputFile
	u32 size = fread(input, 1, 0x70, fd);
	fclose(fd);
	if (size != 0x70) {
		free(input);
		printf("ERROR: %s doesn't have the expected size.\n", inputFile);
		return 2;
	}
	// Allocate output buffer
	Mii* output = malloc(sizeof(Mii));
	if (output == NULL) {
		free(input);
		printf("ERROR: Memory allocation failed.\n");
		return 127;
	}
	// Call APT:Unwrap
	if(!R_SUCCEEDED(APT_Unwrap(0x60, 0x70, 12, 10, input, output))) {
		free(input);
		free(output);
		printf("ERROR: Failed to decrypt %s.\n", inputFile);
		return 1;
	}
	free(input);
	// Open ouputFile
	fd = fopen(outputFile, "w");
	if (fd == NULL) {
		printf("ERROR: Couldn't write to %s.\n", outputFile);
		free(output);
		return 126;
	}
	// Write to outputFile
	size = fwrite(output, 1, 0x60, fd);
	fclose(fd);
	free(output);
	printf("SUCCESS: %s decrypted to %s.\n", inputFile, outputFile);
	return 0;
}

/**
 * Encrypt a Mii (hopefully)
 * @param char* inputFile  Input filename, decrypted Mii
 * @param char* outputFile Output filename, encrypted Mii
 * @return int Return code: 0 means SUCCESS. 1 is APT:Wrap failure.
 *             2 is wrong input. 126 is IO error. 127 is memory allocation.
 */
int encryptMii(const char* inputFile, const char* outputFile) {
	// Open inputFile
	FILE* fd = fopen(inputFile, "r");
	if (fd == NULL) {
		printf("ERROR: Couldn't open %s.\n", inputFile);
		return 126;
	}
	// Allocate input buffer
	Mii* input = malloc(sizeof(Mii));
	if (input == NULL) {
		printf("ERROR: Memory allocation failed.\n");
		return 127;
	}
	// Read inputFile
	u32 size = fread(input, 1, 0x60, fd);
	fclose(fd);
	if (size != 0x60) {
		free(input);
		printf("ERROR: %s doesn't have the expected size.\n", inputFile);
		return 2;
	}
	// Allocate output buffer
	Mii* output = malloc(sizeof(Mii));
	if (output == NULL) {
		free(input);
		printf("ERROR: Memory allocation failed.\n");
		return 127;
	}
	// Call APT:Unwrap
	if(!R_SUCCEEDED(APT_Wrap(0x70, 0x60, 12, 10, input, output))) {
		free(input);
		free(output);
		printf("ERROR: Failed to encrypt %s.\n", inputFile);
		return 1;
	}
	free(input);
	// Open ouputFile
	fd = fopen(outputFile, "w");
	if (fd == NULL) {
		printf("ERROR: Couldn't write to %s.\n", outputFile);
		free(output);
		return 126;
	}
	// Write to outputFile
	size = fwrite(output, 1, 0x70, fd);
	fclose(fd);
	free(output);
	printf("SUCCESS: %s encrypted to %s.\n", inputFile, outputFile);
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
			printf("NIY");
		}
		if (kDown & KEY_R) {
			printf("NIY");
		}
	}
	svcCloseHandle(aptuHandle);
	gfxExit();
	return 0;
}