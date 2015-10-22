#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#define TRUE 1
#define FALSE 0
#define FTPPORT 21

int separateURL(char *url, char* port, char *username, char *password, char *server_url, char *path, char *file){
	
	if(strncmp(url,"ftp://",6) != 0)
		return FALSE;

	unsigned int pos = 0,i=0;
	
	while(url[pos]!=':'){
		port[i]=url[pos];
		pos++;
		i++;	
	}
	port[i]='\0';

	pos = 6;
	i=0;
	while(url[pos]!=':'){
		username[i] = url[pos];
		if(url[pos] == '\0')
			return FALSE;
		i++;
		pos++;
	}
	username[i] = '\n';
	pos++;

	i=0;
	while(url[pos]!='@'){
		password[i] = url[pos];
		if(url[pos] == '\0')
			return FALSE;
		i++;
		pos++;
	}
	password[i] = '\n';
	pos++;

	i=0;
	while(url[pos]!='/'){
		server_url[i] = url[pos];
		if(url[pos] == '\0')
			return FALSE;
		i++;
		pos++;
	}
	server_url[i] = '\0';
	pos++;

	i=0;
	while(1){
		int old_i=i;
		while(url[pos]!='/'){
			path[i] = url[pos];
			if(url[pos] == '\0')
				break;
			i++;
			pos++;
		}
		if(url[pos] == '\0'){
			strncpy(file,path+old_i,i-old_i);
			path[old_i] = '\0';
			break;
		}
		path[i]='/';
		pos++;
		i++;
	}


	return TRUE;
}

int parseDatafd(char* buf, char* ip){
	int i = 27,count=0, pos=0,port;
	char aux[4],aux2[4];
	do{
		if(strncmp(buf+i,",",1)==0){
			if(count==3) break;
			strncpy(ip+pos,".",1);
			count++;
		}
		else{
			strncpy(ip+pos,buf+i,1);
		}
		pos++;
		i++;
	}while(1);

	i++;
	pos=0;
	do{
		if(strncmp(buf+i,",",1)==0)
			break;
		strncpy(aux+pos,buf+i,1);
		i++;
		pos++;
	}while(1);

	i++;
	pos=0;
	do{
		if(strncmp(buf+i,")",1)==0)
			break;
		strncpy(aux2+pos,buf+i,1);
		i++;
		pos++;
	}while(1);

	port = atoi(aux)*256 + atoi(aux2);
	return port;
}

struct hostent* getHost(char *name){
	struct hostent *h;
	if ((h=gethostbyname(name)) == NULL) {  
		herror("gethostbyname");
		perror("Unable to connect to host");
		exit(-1);
	}
	return h;
}

int login(char* username,int usersize ,char* password, int passsize, struct hostent* host){
	int socketfd;
	struct sockaddr_in server_addr;
	char buf[256];

	bzero((char*) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(FTPPORT);
	memcpy(&server_addr.sin_addr, host->h_addr_list[0], host->h_length);

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Unable to open socket.\nsocket()");
		exit(-1);
	}

	if (connect(socketfd, (struct sockaddr *) &server_addr, sizeof(server_addr))< 0) {
		perror("Unable to connect.\nconnect()");
		exit(-1);
	}
	read(socketfd,buf,255);

	if(strncmp(buf,"220",3)!=0){
		perror("Unable to connect.\nconnect()");
		exit(-1);
	}

	write(socketfd,"user ",5);
	write(socketfd, username, usersize);

	bzero(buf,sizeof(buf));
	read(socketfd,buf,255);

	write(socketfd,"pass ",5);
	write(socketfd, password, passsize);

	bzero(buf,sizeof(buf));
	read(socketfd,buf,255);

	if(strncmp(buf,"230",3)!=0){
		printf("Invalid Login\n");
		exit(-1);
	}
	printf("Successfuly logged!\n");

	bzero(buf,sizeof(buf));
	write(socketfd,"\n",1);
	read(socketfd,buf,sizeof(buf));
	return socketfd;
}

int enterPassiveMode(int socketfd){

	char buf[256],ip[128];
	struct sockaddr_in data_addr;
	int datafd,port;

	write(socketfd,"pasv\n",5);
	read(socketfd,buf,sizeof(buf));
	printf("%s\n",buf);
	if(strncmp(buf,"227",3)!=0){
		printf("Error, unable to enter passive mode!\n");
		exit(-1);
	}

	port = parseDatafd(buf,ip);

	bzero((char*) &data_addr, sizeof(data_addr));
	inet_aton(ip, &data_addr.sin_addr);
	data_addr.sin_family = AF_INET;
	data_addr.sin_port = htons(port);

	if ((datafd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Unable to open socket.\nsocket()");
		exit(-1);
	}

	if (connect(datafd, (struct sockaddr *) &data_addr, sizeof(data_addr))< 0) {
		perror("Unable to connect.\nconnect()");
		exit(-1);
	}

	printf("Entered Passive Mode!\n");
	return datafd;
}

int getFileSize(char* buf){
	int i=0;
	do{
		i++;
	}while(strncmp(buf+i,"(",1));

	i++;
	char aux[128];
	int pos=0;
	do{
		strncpy(aux+pos,buf+i,1);
		i++;
		pos++;
	}while(strncmp(buf+i,")",1));
	return atoi(aux);
}

void download(int socketfd,int datafd,char* filepath,char* file){
	char download[256],buf[256];

	bzero(&buf, sizeof(buf));
	sprintf(download, "RETR %s\n", filepath);
	write(socketfd, download, strlen(download));
	read(socketfd, buf, 255);

	if(strncmp(buf,"550",3)==0){
		printf("File doesn't exist.\n");
		exit(-1);
	}
	if(strncmp(buf,"150",3)!=0){
		printf("Error trying to download file.\n");
		exit(-1);
	}
	printf("%s\n",buf);
	int file_size;
	file_size = getFileSize(buf);
	printf("Starting to download file...\n");

	sleep(1);
	FILE *f = fopen(file, "wb");
	char temp_buf[512];
	int bytes,count=0;
	float percent;
	do{
		bytes = read(datafd, temp_buf, 255);
		fwrite(temp_buf, bytes, 1, f);
		count+=bytes;
		percent = 100*((float)count/(float)file_size);
		printf("Writing %d bytes of %d...(%f %)\n",count,file_size,percent);
	}while(bytes>0);
	printf("Download completed!\n");

	fclose(f);
}

int main(int argc, char** argv){
	
	char port[8];
	char username[128];
	char password[128];
	char server_url[128];
	char path[512];
	char file[256];
	char filepath[512];
	
	int socketfd,datafd;

	if(separateURL(argv[1],port,username,password,server_url,path,file)!=TRUE){
		perror("URL invalido");
		exit(-1);
	}
	
	struct hostent *h;
	h = getHost(server_url);
	printf("Nome: %s\n",h->h_name);
	printf("IP: %s\n",inet_ntoa(*((struct in_addr *)h->h_addr)));
	
	socketfd = login(username,strlen(username),password,strlen(password),h);
	datafd = enterPassiveMode(socketfd);
	sprintf(filepath, "%s%s",path,file);
	download(socketfd,datafd,filepath,file);

	close(datafd);
	close(socketfd);
	return 0;
		
}

