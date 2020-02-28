#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>


const size_t BUF_LEN = 1024;
// Something unexpected happened. Report error and terminate.
void sysErr( char *msg, int exitCode )
{
	fprintf( stderr, "%s\n\t%s\n", msg, strerror( errno ) );
	exit( exitCode );
}

// The user entered something stupid. Tell him.
void usage( char *argv0 )
{
	printf( "usage : %s portnumber\n", argv0 );
	exit( 0 );
}

int make_server(char *arg1){
	int listensock = 0;
    struct sockaddr_in serv_addr;                                                                       //new sockaddr_in serv_addr (server address), client_addr
	socklen_t addrLen = sizeof(struct sockaddr_in);                                                     //length of struct sockaddr_in

	if ((listensock = socket(AF_INET, SOCK_STREAM, 0))==-1){                                            //create listensocket --> AF_INET(address family (int this case ipv4)
                                                                                                        //SOCK_STREAM --> connection based protocol, two parties --> tcp
        sysErr("Server Fault: SOCKET", -1);
	}
    printf("[SERVER]: SOCKET successfull: %i\n",listensock);                                            //Debug: print socketnumber

	// Set params so that we receive IPv4 packets from anyone on the specified port
	memset( &serv_addr, 0, addrLen );                                                                   //fill memory block with sockaddr_in serv_addr with length of it
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);                                                      //accept all connections --> ip: 0.0.0.0
	serv_addr.sin_family      = AF_INET;                                                                //fill address family, in this case ipv4
	serv_addr.sin_port        = htons( (u_short)atoi( arg1 ) );                                         //fill port as the network-byte-order prefers --> this case 34343

	if (bind(listensock, (struct sockaddr*) &serv_addr, sizeof(serv_addr))==-1){                        //bind to address 0.0.0.0
        sysErr("Server Fault: BIND",-2);                                                                //if failed --> print it to user
	}
    printf("[SERVER]: BIND successful to address: %s\n", inet_ntoa(serv_addr.sin_addr));                //Debug: print that bind was successful


	if (listen(listensock,10)==-1){                                                                     //listen on listensocket to new connetion wishes, queue size 10
        sysErr("Server Fault: LISTEN", -3);                                                             //if failed --> print to user
	}
    printf("[SERVER]: LISTEN successful \n");                                                           //Debug: print that listen was successful
    return listensock;
}


void processRequest(int commsock){
    int file_descript, len, fileread, rcvd;
    char revBuff[BUF_LEN], sendbuffer[BUF_LEN];                                                         //buffer to recieve and send data
    char *firstline, *url, *content_type;
    char notFound_return[] = "HTTP/1.0 404 NOT FOUND\r\nContent-Type: text/html\r\nServer: MicroWWW \r\n\r\n <html><body><h1>404 File Not Found</h1></body></html>\r\n";
    char good_return[] = "HTTP/1.0 200 OK\r\n"; //<html><body>Thats a fake site!</body></html>\r\n";
    char bad_return[] = "HTTP/1.0 501 NOT IMPLEMENTED\r\nContent-Type: text/html\r\nServer: MicroWWW \r\n\r\n <html><body><b>501</b> Operation not supported</body></html>\r\n";

    rcvd = recv(commsock, revBuff, BUF_LEN, 0);                                 //recieve request from communication socket
    if (rcvd < 0){                                                              //if lower 0, something failed
        sysErr("[processRequest] recieve failed",3);                            //print Error
    }else if (rcvd == 0){                                                       //if 0, client closed connection
        sysErr("[processRequest] Client closed socket",3);                      //print Error
    }else{                                                                      //else recieved successful
        //printf("[processRequest] revBuff %s",revBuff);
        if((firstline = strtok(revBuff, " \r\n")) != NULL ){                    //read first line of request --> looks like this: GET /url  HTTP1.1
            //printf("[processRequest] Request-Type is: %s \n",revBuff);

            if(strcmp(firstline,"GET") == 0){                                   //if GET in first line do the following
                //printf("[processRequest] Found GET: %s\n",firstline);
                url = strtok(NULL, " \r\n");                                    //we online need the url, split it
                //printf("[processRequest] before url is: %s \n",url);
                if (url == NULL) sysErr("NO URL: ",2);                          //if url in null, print Error
                if(!strncmp(url, "/",strlen(url))) strcpy(url, "/index.html");  //if url only "/" then print the index.html file
                if (url[0] == '/' ) url = &url[1];                              //remove slash
                if(!strncmp(url, "/..",strlen(url))) strcpy(url, "/index.html");
                //printf("[processRequest] url is: %s \n",url);

                char *www = "/var/microwww";                                    //set homedir for websites, should be /var/microwww
                char *url1 = (char *) malloc(1+strlen(url)+strlen(www));        //new variable to concatenate the homedir with url, set disk space
                strcpy(url1, www);                                              //copy homemdir in new variable
                strcat(url1, url);                                              //concatenate the homedir and the url
                strncpy(url, url1, strlen(url)+strlen(url1));                   //copy back to url, strncpy will prevent buffer overflow
                free(url1);                                                     //free the reserved disk space
                //printf("[processRequest] new url: %s\n",url);

                len = strlen(url);                                              //length of the url
                content_type = "text/plain";                                    //set default content type to plaintext
                if (strcmp(&url[len-5], ".html") == 0) content_type="text/html";    //compare end of url and file ending, adjust the content-type
                if (strcmp(&url[len-4], ".htm") == 0) content_type="text/html";
                if (strcmp(&url[len-4], ".jpg") == 0) content_type="image/jpeg";
                if (strcmp(&url[len-5], ".jpeg") == 0) content_type="image/jpeg";
                if (strcmp(&url[len-4], ".png") == 0) content_type="image/png";

                if ((file_descript = open(url, O_RDONLY )) < 0){                //if return value of open the url file is lower 0
                    write(commsock,notFound_return, strlen(notFound_return));   //the file doesnt exist, send 404 message
                    close(file_descript);                                       //close the file
                }else{                                                          //else the file exists
                    snprintf(sendbuffer, BUF_LEN, "%sContent-Type: %s\r\nServer: MicroWWW Team 10 \r\n\r\n", good_return, content_type);    //create 200 OK message, snprintf will prevent buffer overflow
                    write(commsock, sendbuffer, strlen(sendbuffer));            //send 200 OK message

                    while ( (fileread = read(file_descript, sendbuffer, sizeof(sendbuffer))) > 0){    //read the file as long as the file is
                        write(commsock, sendbuffer, fileread);                     //send the file
                    }
                    close(file_descript);                                       //after sending file, close file reader
                }
            }else{                                                              //if request type isn´t GET, e.g. POST
                write(commsock,bad_return, strlen(bad_return));                 //send 501 BAD_RETURN message
                return;                                                         //return back to main
            }
        }
    }
    return;
}


int main(int argc, char **argv)
{
	int commsock, listensock;
	pid_t pid;                                                                  //initialisze listensocket, communication socket
    struct sockaddr_in client_addr;
    socklen_t addrLen = sizeof(struct sockaddr_in);

	// Check for right number of arguments
    if ( argc < 2 ) usage( argv[0] );

	listensock = make_server(argv[1]);                                          //create listensocket with function make_server(port)

    signal(SIGCHLD, SIG_IGN);                                                   //to prevent zombie process
	while(true){
        //printf("[MAIN]: Begin while\n");

        if ((commsock = accept(listensock, (struct sockaddr*)&client_addr, &addrLen)) == -1){   //create communication socket, used to connection with client
            sysErr("Server Fault: ACCEPT", -2);                                 // if failed --> print it
        }

        switch(pid = fork()){                                                   //make childprocess
            case -1:    close(commsock); sysErr("Server fault: FORK", -2); break;   //if -1 than fork failed
            case 0:     processRequest(commsock);                               //if 0 it is a child process
        }
        //printf("[SERVER]: Got it and sent!: %s\n", revBuff);                  //Debug: print, that recieved and sent back to client
        close(commsock);                                                        //close communication socket
	}
    close(listensock);                                                          //close listen socket
	return 0;
}
