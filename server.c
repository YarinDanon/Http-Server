//Yarin Danon
//ID: 305413122
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <sys/shm.h>
#include <math.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>

#include "threadpool.h"

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define MAX_LENGHT 4000
#define ERR "HTTP/1.0 %s\r\nServer: webserver/1.0\r\nDate:%s\r\n%sContent-Type: text/html\r\nContent-Length: %s\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>%s</TITLE></HEAD>\r\n<BODY><H4>%s</H4>\r\n%s\r\n</BODY></HTML>"
#define DIR_CONTENT "HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Type: text/html\r\nContent-Length: %ld\r\nLast-Modified: %s\r\nConnection: close\r\n\r\n<HTML>\r\n<HEAD><TITLE>Index of %s</TITLE></HEAD>\r\n<BODY>\r\n<H4>Index of %s</H4>\r\n<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n"
#define FILE_OK "HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: %s\r\n%sContent-Length: %ld\r\nLast-Modified: %s\r\nConnection: close\r\n\r\n"

int work(void* data);
void usage();
void errType(char* type , int fd ,char** path, int counter);
void writeToFd(int fd,char** arr);
void openFile(char* html , int fd , char** arr);
int validNum(const char* str);
char *get_mime_type(char *name);
void inDirectory(int fd , char** arr);
int confirmtionAccess(char * str);


char finalStr [450];
long int numOfEntity ;

int main(int argc, char const *argv[])
{
	memset(finalStr,'\0',450);
	//usage
	if(argc != 4)
	{
		usage();
	}
	//creat threadpool and check the argv array
	threadpool* pool ;
	for(int i = 1 ; i < argc ; i++)
	{
		if(validNum(argv[i]) == -1 ||atoi(argv[i]) < 1 )
		{
			usage();
		}
	}
	pool = create_threadpool(atoi(argv[2]));

	if(pool == NULL)
	{
		usage();
	}
	
	//initialization of the socket .    
	int sockfd, newsockfd;    
	struct sockaddr_in serv_addr; 
	struct sockaddr_in cli_addr; 
	
	socklen_t addrLen = sizeof(cli_addr);   	
//========================== connection infrastructure ======================

	sockfd = socket(PF_INET,SOCK_STREAM,0); 
	if (sockfd < 0)  
	{
		perror("ERROR opening socket\n");
		destroy_threadpool(pool);
		exit(0);
	} 

	serv_addr.sin_family = AF_INET;  
	serv_addr.sin_addr.s_addr = INADDR_ANY; 
	serv_addr.sin_port = htons(atoi(argv[1])); 

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)  
	{               
		perror("ERROR on binding\n");
		destroy_threadpool(pool);
		exit(0);
	}
	listen(sockfd,atoi(argv[3]));
	//array of FD 
	int* fdArr = (int*)malloc(atoi(argv[3])*sizeof(int));
	if(fdArr == NULL)
	{
		perror("ERROR in malloc\n");
		destroy_threadpool(pool);
		exit(0);
	}
	//Connecting to the client and send to funcion 
	for(int i = 0 ; i < atoi(argv[3]) ; i++)
	{
		newsockfd = accept(sockfd,(struct sockaddr*)&cli_addr,&addrLen);
		if(newsockfd < 0)
		{
			printf("ERR accecpt\n");
			continue;
		}
		fdArr[i] = newsockfd;
		dispatch(pool,work,&fdArr[i]);
	}
	destroy_threadpool(pool);
	free(fdArr);
	//close the socket;
	close(sockfd);
	
}
//usage if the input are not valid
void usage()
{
	printf("server <port> <pool-size> <max-number-of-request>Port\n");
	exit(0);
}
//the main function
int work(void* data)
{
	memset(finalStr,'\0',450);
	char rbuff[MAX_LENGHT];
	memset(rbuff,'\0',MAX_LENGHT);
	int* fd = (int*)data;
	int readResponse = 0;
	//read from the first line of the requerst client
	while(read(*fd,rbuff+readResponse,1)>0)
	{
		if(strcmp(rbuff+readResponse,"\n") == 0)
		{
			break;
		}
		readResponse++;
	}
	char *token;
    token = strtok(rbuff," ");
    int counter = 0;
	char* arr [3];
	//puts the tokens of the request in array
    while( token != NULL ) 
	{
		// case of too many tokens
		if(counter > 2)
		{
			errType("400",*fd,arr,counter);
			return 1;
		}
		arr[counter] = (char*)malloc(strlen(token)+1);
		//case of system ERR
 		if(arr[counter] == NULL)
		{
			errType("500",*fd,arr,counter);
			return 1;
		}
		memset(arr[counter],'\0',strlen(token)+1);
		strcpy(arr[counter],token);
    	token = strtok(NULL," ");
		counter++;
    }
	//check the Protocol
	if(counter != 3 || strcmp(arr[2],"HTTP/1.1\r\n")!=0)
	{
		//bad request
		errType("400",*fd,arr,counter);
		return 1;		
	}
	//if the path is "/"
	if(strcmp(arr[1],"/") == 0 )
	{
		free(arr[1]);
		arr[1] = (char*)malloc(4);
		if(arr[1] == NULL)
		{
			errType("500",*fd,arr,counter);
			return 1;
		}
		memset(arr[1],'\0',4);
		strcpy(arr[1],"/./");		
	}
	//if the method is not GET
	if( strcmp(arr[0],"GET") != 0 )
	{
		errType("501",*fd,arr,counter);
		return 1;
	}
	//if the file not found
	if(access(arr[1]+1,F_OK) == -1 && strcmp(arr[1],"") != 0)
	{
		errType("404",*fd,arr,counter);
		return 1;
	}
	struct stat sb;
	stat(arr[1]+1,&sb);
	//case of folder
	if((sb.st_mode & S_IFMT)== S_IFDIR)
	{
		//case of the lasr char not '/'
		if( arr[1][strlen(arr[1]+1)] != '/' )
		{
			errType("302",*fd,arr,counter);
			return 1;
		}
		//serch for the index.html
		char* html = (char*)malloc(strlen(arr[1])+12);
		if(html == NULL)
		{
			errType("500",*fd,arr,counter);
			return 1;
		}
		memset(html,'\0',strlen(arr[1])+12);
		strcpy(html,arr[1]);
		strcat(html,"index.html");
		//index.html exists
		if( access( html+1, F_OK ) != -1 ) 
		{
			//case of no Permissions
			if(confirmtionAccess(html+1) != 1)
			{	
				errType("403",*fd,arr,counter);
				free(html);
				return 1;
			}
			openFile(html,*fd,arr);
			//close fd and free the array
    		close(*fd);
			for(int i = 0 ; i < counter ; i++)
			{
				free(arr[i]);
			}
			free(html);
			return 0;
		}
		//index.html not exists
		else 
		{
			free(html); 
			//case of no Permissions
			if(confirmtionAccess(arr[1]+1) != 1)
			{	
				errType("403",*fd,arr,counter);
				return 1;
			}
			//creat the table
			inDirectory(*fd,arr);
		}
	}
	else if(S_ISREG(sb.st_mode))
	{
		if(confirmtionAccess(arr[1]+1) == 1){
			openFile(arr[1] , *fd , arr);
		}
		else{
			errType("403",*fd,arr,counter);
			return 1;
		}
	}
	//case of non Regular file and Permissions not exists
	else
	{
		errType("403",*fd,arr,counter);
		return 1;
	}
	//close fd and free the array
	close(*fd);
	for(int i = 0 ; i < counter ; i++)
	{
		free(arr[i]);
	} 
	return 0;
}

char *get_mime_type(char *name) 
{
	char *ext = strrchr(name, '.');
	if (!ext) return NULL;
	if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
	if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
	if (strcmp(ext, ".gif") == 0) return "image/gif";
	if (strcmp(ext, ".png") == 0) return "image/png";
	if (strcmp(ext, ".css") == 0) return "text/css";
	if (strcmp(ext, ".au") == 0) return "audio/basic";
	if (strcmp(ext, ".wav") == 0) return "audio/wav";
	if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
	if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
	if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
	return NULL;
}

//method that riceve the error type and write the format
void errType(char* type , int fd ,char** arr, int counter)
{
	time_t now;
	char timebuf[128];
	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
	char* location;
	//bad request
	if(strcmp(type,"400") == 0)
	{
		sprintf(finalStr,ERR,"400 Bad Request",timebuf,"","113","400 Bad Request","400 Bad Request","Bad Request.");
	}
	//not supported
	else if(strcmp(type,"501") == 0)
	{
		sprintf(finalStr,ERR,"501 Not supported",timebuf,"","129","501 Not supported","501 Not supported","Method is not supported.");
	}
	//not found
	else if(strcmp(type,"404") == 0)
	{
		sprintf(finalStr,ERR,"404 Not Found",timebuf,"","112","404 Not Found","404 Not Found","File not found.");
	}
	//system ERR
	else if(strcmp(type,"500") == 0)
	{
		sprintf(finalStr,ERR,"500 Internal Server Error",timebuf,"","144","500 Internal Server Error","500 Internal Server Error","Some server side error.");
	}
	//Directories must end with a slash.
	else if(strcmp(type,"302") == 0)
	{
		location = (char*)malloc(14+strlen(arr[1]));
		if(location == NULL)
		{
			errType("500",fd,arr,counter);
			return;
		}
		strcpy(location,"Location: ");
		strcat(location,arr[1]);
		strcat(location,"/\r\n");
		sprintf(finalStr,ERR,"302 Found",timebuf,location,"123","302 Found","302 Found","Directories must end with a slash.");
	}
	//dir_content
	if(strcmp(type,"dir_content") == 0)
	{
		struct stat sb;
    	stat(arr[1]+1, &sb);
		char  update [128];
    	strftime(update, sizeof(update), RFC1123FMT, gmtime(&sb.st_mtime));
		sprintf(finalStr,DIR_CONTENT,timebuf,numOfEntity,update,arr[1]+1,arr[1]+1);
		writeToFd(fd,arr);
		return;
	}
	//non Permissions
	if(strcmp(type,"403") == 0)
	{
		sprintf(finalStr,ERR,"403 Forbidden",timebuf,"","111","403 Forbidden","403 Forbidden","Access denied.");
	}
	//write the right format to the right FD
	
	writeToFd(fd,arr);
	for(int i = 0 ; i < counter ; i++)
	{
		free(arr[i]);
	}
	if(strcmp(type,"302") == 0)
	{
		free(location);
	}
	close(fd); 
}
//write to FD
void writeToFd(int fd,char** arr)
{
	int len = strlen(finalStr);
	int num = 0;
	int sum = 0;
	//loop for write the request
	do  
    {
		num = write(fd,sum+finalStr,strlen(finalStr)+2-sum);
        if (num < 0)    
		{	
			errType("500",fd,arr,3);
			return;
		}
		if(num == 0)
			break;		
		sum+=num;
    }while(sum<len);
}
//method that open file
void openFile(char* html , int fd ,char** arr)
{
	FILE *fp;
	char ch;
    fp = fopen (html+1,"r");
    if (fp == NULL) 
	{
        errType("500",fd,arr,3);
        return ;
    }
	
	time_t now;
	char timebuf[128];
	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
	struct stat sb;
    stat(arr[1]+1, &sb);
	char  update [128];
    strftime(update, sizeof(update), RFC1123FMT, gmtime(&sb.st_mtime));
	//malloc fo the size of the file
	char* page =(char*) malloc(sb.st_size+1);
    memset(page,'\0',sb.st_size+1);
	//if no type
	if(get_mime_type(arr[1]) == NULL)
	{
		sprintf(finalStr,FILE_OK,timebuf,"",sb.st_size,update);
		writeToFd(fd,arr);	
	}
	else
	{
		int len = 15+strlen(get_mime_type(arr[1]))+1;
		char* mineType = (char*)malloc(len+1);
		if(mineType == NULL)
		{
			errType("500",fd,arr,3);
			return ;
		}
		memset(mineType,'\0',len);
		strcpy(mineType,"Content-Type: ");
		strcat(mineType,get_mime_type(arr[1]+1));
		strcat(mineType,"\r\n");
		sprintf(finalStr,FILE_OK,timebuf,mineType,sb.st_size,update);
		writeToFd(fd,arr);
		free(mineType);	
	}	
	//write the file
	int res = 0;
    while(res = fread(page,1,sb.st_size,fp)>0)
    {
    	write(fd,page,sb.st_size);
	}
	fclose (fp);
	free(page);

}
//if the string is number
int validNum(const char* str)
{
	for(int i = 0 ; i < strlen(str);i++)
	{
		if(isdigit(str[i]) == 0)
			return -1;
	}
	return 1;
}
//creat table of the folder
void inDirectory(int fd , char** arr)
{
	time_t now;
	char timebuf[128];
	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
	struct stat sb;
    stat(arr[1]+1, &sb);
	char  update [128];
	memset(update,'\0',128);
	struct dirent **namelist;
	int n = 0;
	//scan folder
	n = scandir(arr[1]+1, &namelist, NULL, NULL);
	//system ERR
	if (n == -1) 
	{
		errType("500",fd,arr,3);
		return ;
	}
	//size of 500* number of entinty
	char* directorySize = (char*)malloc((n)*500+1);
	//system ERR
	if(directorySize == NULL)
	{
		errType("500",fd,arr,3);
		return ;
	}
	memset(directorySize,'\0',(n)*500+1);
	char entinty [500];
	//create line foe each entinty
	while (n--) 
	{
		char* temp = (char*)malloc(strlen(arr[1]+1)+strlen(namelist[n]->d_name)+1);
		//system ERR
		if(temp == NULL)
		{
			errType("500",fd,arr,3);
			return ;
		}
		memset(temp,'\0',strlen(arr[1]+1)+strlen(namelist[n]->d_name)+1); 
		strcat(temp,arr[1]+1);
		strcat(temp,namelist[n]->d_name);
		strftime(update, sizeof(update), RFC1123FMT, gmtime(&sb.st_mtime));
		stat(temp, &sb);
		//case of folder
		if((sb.st_mode & S_IFMT)== S_IFDIR)
		{
			sprintf(entinty,"<tr><td><A HREF=\"%s/\">%s</A></td><td>%s</td><td></td></tr>\r\n",namelist[n]->d_name,namelist[n]->d_name,update);
		}
		//case if file
		else
		{
			sprintf(entinty,"<tr><td><A HREF=\"%s\">%s</A></td><td>%s</td><td>%ld</td></tr>\r\n",namelist[n]->d_name,namelist[n]->d_name,update,sb.st_size);
		}
		strcat(directorySize,entinty);
    	free(namelist[n]);
		free(temp);
    }
	strcat(directorySize,"</table>\r\n<HR>\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n</BODY></HTML>");
	numOfEntity = (long int)strlen(directorySize);
	//send to errType method 
	errType("dir_content",fd,arr,3);
	int len = strlen(directorySize);
	int num = 0;
	int sum = 0;
	//write to the fd
	do  
    {
		num = write(fd,sum+directorySize,strlen(directorySize)-sum);
        if (num < 0)    
		{	
			errType("500",fd,arr,3);
			return;
		}
		if(num == 0)
			break;		
		sum+=num;
    }while(sum<len);

    free(namelist);
	free(directorySize);
}
//confirmtion Access, get the path and parse the path for each '/'
int confirmtionAccess(char * str)
{
	char *token;
 	char path[MAX_LENGHT]; 
	memset(path,'\0',MAX_LENGHT);
	char partOfPath[MAX_LENGHT];
	memset(partOfPath,'\0',MAX_LENGHT); 
	strcpy(path,str);
	struct stat statbuf;
   	stat(str, &statbuf); 
	//parse for each '/'   
	token = strtok(path, "/"); 
	while( token != NULL) 
    {
		strcat(partOfPath,token); 
		stat(partOfPath, &statbuf); 
		if(S_ISDIR(statbuf.st_mode) && !(statbuf.st_mode &S_IXOTH)) 
			return 0;
		else if(!(statbuf.st_mode & S_IROTH)) 
			return 0;
    	token = strtok(NULL, "/"); 
    	strcat(partOfPath,"/");
    }
    return 1;
}