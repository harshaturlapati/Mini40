/* Simple demo showing how to communicate with Net F/T using C language. */

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>
#include <process.h>
#include <io.h>

void format_exp_time(char *output){
    time_t rawtime;
    struct tm * timeinfo;
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    sprintf(output, "\\data\\%d-%d-%d.%d_%d_%d.", timeinfo->tm_year + 1900,
            timeinfo->tm_mon + 1, timeinfo->tm_mday,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

int main ( ) {

	DWORD nBufferLength = MAX_PATH;
	char data_path[MAX_PATH + 1];
	char exp_folder[MAX_PATH + 1];
	GetCurrentDirectory(nBufferLength, data_path); 
	data_path[MAX_PATH +1 ] = '\0';
	
	sprintf (exp_folder, "%s", data_path);
	strcat(exp_folder, "\\data\\");
    int result_mkdir = CreateDirectory(exp_folder, NULL);
	printf("Result from mkdir = %d\n",result_mkdir);
	//mkdir(data_path);
	
	char sentence[1000];

    // creating file pointer to work with files
    FILE *fptr;

    // opening file in writing mode
    fptr = fopen("exp_settings.txt", "w");

    // exiting program 
    if (fptr == NULL) {
        printf("Error!");
        exit(1);
    }
	
	struct timespec ts;

	char buff[100];
	char exp_foldername[100];

	time_t rawtime;
	struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	sprintf (exp_foldername, "Current local time and date: %s", asctime (timeinfo) );
  
	format_exp_time(exp_foldername);
  
	printf("Define the experiment details : exp_name.subject_name\n");
	fgets(sentence, sizeof(sentence), stdin);
	//printf("sentence is : %s \n",sentence);
	strcat(exp_foldername,sentence);
	//printf("exp_foldername is : %s \n",exp_foldername);
	
	int exp_foldername_len = strlen(exp_foldername);
	exp_foldername[exp_foldername_len-1] = '\0';

	strcat(data_path,exp_foldername);
	strcat(data_path,"\\");
	//printf("data_path is : %s \n",data_path);
	
	int result_mkdir2 = CreateDirectory(data_path, NULL);
	printf("Result from mkdir 2 = %d\n",result_mkdir2);

		printf("Experiment folder made is : %s \n",data_path);
		//fprintf(fptr, "%s", exp_foldername);
		//char *pointer;
		//pointer = exp_foldername;
		fputs(exp_foldername,fptr);

	fclose(fptr);

    
	return 0;
}

