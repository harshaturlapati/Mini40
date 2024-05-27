/* Simple demo showing how to communicate with Net F/T using C language. To be used after running define_exp_v4.exe. */

#include <winsock2.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <conio.h>
#include <sys/stat.h>
#include <io.h>
#include <inttypes.h>
#include <windows.h>
#include <process.h>

#define PORT 49152 /* Port the Net F/T always uses */
#define COMMAND 2 /* Command code 2 starts streaming */
#define NUM_SAMPLES 1 /* Will send 1 sample before stopping */

#define WIN32_LEAN_AND_MEAN
#include <Ws2tcpip.h>

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define SEND_PORT 27017

// Include for RECV
#ifndef UNICODE
#define UNICODE
#endif


// Definition of signals to enable Ctrl + C exit to the C while loop
volatile sig_atomic_t stop;

void
inthand(int signum)
{
    stop = 1;
}

// Function definition to convert system time to Unix epoch with nanoseconds precision

double
epoch_double(struct timespec *tv)
{
  char time_str[32];

  sprintf(time_str, "%ld.%.9ld", tv->tv_sec, tv->tv_nsec);

  return atof(time_str);
}


// ATI Mini40 NETBA Software declarations
/* Typedefs used so integer sizes are more explicit */
typedef unsigned int uint32;
typedef int int32;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned char byte;
typedef struct response_struct {
	uint32 rdt_sequence;
	uint32 ft_sequence;
	uint32 status;
	int32 FTData[6];
} RESPONSE;


// Main code
int main () {


// Declare and initialize variables - SEND
    int iResult;

    SOCKET ConnectSocket = INVALID_SOCKET;
    struct sockaddr_in clientService;
    //int SenderAddrSize = sizeof(clientService);

    char* sendbuf = "this is a test";
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
	
	// Mini40 stuff
		
	printf("Enter IP Address for the ATI to record\n");
    char IP_ADDRESS[20];
    scanf("%[^\n]%*c", IP_ADDRESS);
    printf("IP Address entered is : %s\n", IP_ADDRESS);
	
	printf("Enter the ATI #\n");
    char ATI_ID[20];
    scanf("%[^\n]%*c", ATI_ID);
    printf("ATI ID is : %s\n", ATI_ID);

	int socketHandle;			/* Handle to UDP socket used to communicate with Net F/T. */
	struct sockaddr_in addr;	/* Address of Net F/T. */
	struct hostent *he;			/* Host entry for Net F/T. */
	byte request[8];			/* The request data sent to the Net F/T. */
	RESPONSE resp;				/* The structured response received from the Net F/T. */
	byte response[36];			/* The raw response data received from the Net F/T. */
	int i;						/* Generic loop/array index. */
	int err;					/* Error status of operations. */
	char * AXES[] = { "Fx", "Fy", "Fz", "Tx", "Ty", "Tz" };	/* The names of the force and torque axes. */
	WSADATA wsaData;



	// Initialize Winsock version 2.2

   if( WSAStartup(MAKEWORD(2,2), &wsaData) != 0){

        printf("Server: WSAStartup failed with error: %ld\n", WSAGetLastError());

        return -1;
   }
   else{
        printf("Server: The Winsock DLL status is: %s.\n", wsaData.szSystemStatus);
   }
   
    // Create a SOCKET for connecting to server - SEND
    ConnectSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ConnectSocket == INVALID_SOCKET) {
        printf("Error at socket(): %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
	
	// UDP send - testing
	// The sockaddr_in structure specifies the address family,
    // IP address, and port of the server to be connected to. - SEND
    clientService.sin_family = AF_INET;
    clientService.sin_addr.s_addr = inet_addr("127.0.0.1");
    clientService.sin_port = htons(SEND_PORT);
   
	/* Calculate number of samples, command code, and open socket here. */
	socketHandle = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketHandle == -1) {
		exit(1);
	}
	
	 // Connect to server.
    iResult = connect(ConnectSocket, (SOCKADDR*)&clientService, sizeof(clientService));
    if (iResult == SOCKET_ERROR) {
        closesocket(ConnectSocket);
        printf("Unable to connect to server: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Send an initial buffer
    iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
    if (iResult == SOCKET_ERROR) {
        printf("send failed: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    printf("Bytes Sent: %ld\n", iResult);
	
	// UDP send - testing ends
	
	*(uint16*)&request[0] = htons(0x1234); /* standard header. */
	*(uint16*)&request[2] = htons(COMMAND); /* per table 9.1 in Net F/T user manual. */
	*(uint32*)&request[4] = htonl(NUM_SAMPLES); /* see section 9.1 in Net F/T user manual. */
	
	/* Sending the request. */
	he = gethostbyname(IP_ADDRESS);
	memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	
	err = connect( socketHandle, (struct sockaddr *)&addr, sizeof(addr) );
	if (err == -1) {
		exit(2);
	}
	
	/* Retrieving the experiment details */
	FILE    *textfile;
    char    *exp_folder;
    long    numbytes;
	textfile = fopen("exp_settings.txt", "r");
    if(textfile == NULL)
        return 1;
     
    fseek(textfile, 0L, SEEK_END);
    numbytes = ftell(textfile);
    fseek(textfile, 0L, SEEK_SET);  
 
    exp_folder = (char*)calloc(numbytes, sizeof(char));   
    if(exp_folder == NULL)
        return 1;
 
    fread(exp_folder, sizeof(char), numbytes, textfile);
    fclose(textfile);
 
    printf("Exp folder read from exp_settings.txt is : %s\n",exp_folder);
	DWORD nBufferLength = MAX_PATH;
	char szCurrentDirectory[MAX_PATH + 1];
	GetCurrentDirectory(nBufferLength, szCurrentDirectory); 
	szCurrentDirectory[MAX_PATH +1 ] = '\0';
	printf("Current directory is : %s\n",szCurrentDirectory);
	strcat(szCurrentDirectory,exp_folder);
	printf("Current directory is : %s\n",szCurrentDirectory);
	
	// creating file pointer to work with files
    FILE *fptr;
	char ati_number[100]; 
	sprintf(ati_number,"%s",ATI_ID);

	//printf("Experiment folder is 1: %s\n", exp_folder);
	strcat(szCurrentDirectory,"\\");
	printf("%s",ati_number);
	//printf("Experiment folder is 2: %s\n", exp_folder);
	strcat(szCurrentDirectory,ati_number);
	int result = CreateDirectory(szCurrentDirectory, NULL);
	//printf("MKDIR result was : %d", result);
	//printf("Experiment folder is 3: %s\n", exp_folder);
	strcat(szCurrentDirectory,"\\ati_data.txt");
	//printf("Experiment folder is 4: %s\n", exp_folder);
    // opening file in writing mode
    fptr = fopen(szCurrentDirectory, "w");
	printf("File to be written in %s\n",szCurrentDirectory);
    // exiting program 
    if (fptr == NULL) {
        printf("Error!");
        exit(1);
    }
	
	
	// Defining variables for obtaining system time and ati data
	struct timespec ts;
  
	char curr_data[500];
	char ati_field[500];
	char unix_epoch[500];
	char ati_data[500];

	int32 dummy_var;
	int size;
	
	// Defining the interrupt signal for keeping an eye on Ctrl+C input by user
	signal(SIGINT, inthand);
	
	while(!stop)
	{
		send( socketHandle, request, 8, 0 );

		/* Receiving the response. */
		recv( socketHandle, response, 36, 0 );
		clock_gettime(CLOCK_REALTIME, &ts);

		// Packaging
		resp.rdt_sequence = ntohl(*(uint32*)&response[0]);
		resp.ft_sequence = ntohl(*(uint32*)&response[4]);
		resp.status = ntohl(*(uint32*)&response[8]);
		for( i = 0; i < 6; i++ ) {
			resp.FTData[i] = ntohl(*(int32*)&response[12 + i * 4]);
		}


		/* Output the response data. */
		//printf( "Status: 0x%08x\n", resp.status );
		sprintf(ati_data,"||");
		for (i =0;i < 6;i++) {
			//printf("%s: %d\n", AXES[i], resp.FTData[i]);
			dummy_var = resp.FTData[i];
			__builtin_bswap32(dummy_var);
			sprintf(ati_field,"%s: %.7f||", AXES[i], (double) dummy_var/1000000);
			strcat(ati_data,ati_field);
		}
		
	sprintf(unix_epoch,"t: %.9f\n", epoch_double(&ts));
	strcat(curr_data,ati_data);
	strcat(curr_data,unix_epoch);
	printf("%s",curr_data);
	size = strlen(curr_data);
	curr_data[size] = '\0';
	//fprintf(fptr, "%s", curr_data);
	wprintf(L"Sending datagrams...\n");

        iResult = send(ConnectSocket, curr_data, (int)strlen(curr_data), 0);
        wprintf(L"datagrams sent...\n");
		
	fputs(curr_data,fptr);
	ati_data[0] = '\0';
	curr_data[0] = '\0';
	}
	
    // cleanup - SEND
    closesocket(ConnectSocket);
	// Saving data
	printf("exiting safely\n");
	WSACleanup();
	fclose(fptr);
	return 0;
}
