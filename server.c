
/**
 * Duoduo Lin 21884786
 * Guangming Chen 21664707
 * 
 * Test environment: CSSE MAC
 * compile with make, then ./server 4444
 * 
 **/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>



//Define two structs for passing arguments to pthread function

struct client_info {
    int sockno;
    char ip[INET_ADDRSTRLEN];
};

struct message{
    int clientfd;
    char *mess;
};



int maxplayer;
int *live ;
int *preid;
int clients[100];      //array to store client's fd
int id[100]={0};       //array to store client id
int conn[100]={0};     //store contain number given by clients
char *action[50];      //store action given by clients
bool gaming = false;   
int n = 0;
int m = 0;
int x;
int y;
int z;
int lateplayer=0;
int playerCount=0;
int pcount;
int p = 0;
int numdigit;
int dice1;
int dice2;
int sum;
int tempid1 = 0;
int tempid2 = 0;
int idcount = 0;
int digitcount;
int initialID = 100;
int winner = 0;
char *buf6;
char *buf7;
char *buf8;
int msgCount = 0 ;         
int rStart;             
pthread_t splitt,rClock,cmsg;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;



/**
 * function that send message to all clients
 * */

void sendtoall(char *msg,int curr)  
{
    
    
    int i;

    for(i = 0; i < n; i++) {
        
        if(send(clients[i],msg,strlen(msg),0) < 0) {
            perror("sending failure");
            continue;
            
        }
    }

}

/**
* function that send message to single particular client
* */

void sendtoown(char *msg,int curr)  
{
    if(curr==0){
        if(send(clients[n],msg,strlen(msg),0)<0){
            perror("sending failure");
            
        }
    }
    printf("winner: %i\n", winner);               //print out number of clients ELIM at the same time
    if(winner==playerCount&&playerCount>1){       //if more than one clients are remove at the same time
        buf7 = calloc(1024, sizeof(char));
        sprintf(buf7,"%i,VICT",preid[curr]);
        sendtoall(buf7, 0);
    }
    else{
        
        int i;
        pthread_mutex_lock(&mutex);
        winner=0;
        for(i = 0; i < n; i++) {
            if(clients[i]==curr){
                if(send(clients[i],msg,strlen(msg),0) < 0) {
                    perror("sending failure");
                    continue;
                }
            }
        }
        pthread_mutex_unlock(&mutex);
    }

}



void *countdown(void *arg){
    
/**
 * to allow clients enter lobby within 20 seconds, if not enough clients, close the game.
 * */ 
    sleep(20);
   
    
    if(n<maxplayer&&gaming==false){                  
        buf6 = calloc(1024, sizeof(char));
        buf6[0] = '\0';
        sprintf(buf6,"CANCEL");
        sendtoall(buf6,n);
        exit(EXIT_FAILURE);
        
    }
    return (0);
}


void *roundClock(void *arg){

/**
 * Set 8s each round, if clients fail to send message during this time ,it will recieved penalty
 **/
    
    pthread_mutex_lock(&mutex);
    pcount=playerCount;                                //a variable to store the current number of players
    pthread_mutex_unlock(&mutex);
    
    rStart = 0;                                         //initialize flag to 0

    for(int i=0;i<8;i++){                               //message timer, countdown 8 seconds.
        sleep(1);                                       

        if(msgCount==playerCount){                     //if server recived all the messages from clients in 8s 
            break;
        }
        if(playerCount==0){
            printf("Game finish!\n");
            exit(EXIT_FAILURE);
        }
    }

    if(msgCount<playerCount){                           //if times up and recived messages are fewer than playerCount, set flag to 1
        rStart =1;
    }
    
    else {
        rStart = 0;
        pthread_cond_broadcast(&cond1);                //broadcast() to wait(), so that the game continues
    }
   
 
    if(playerCount==1){
       pthread_cond_broadcast(&cond1);
    }
    if(playerCount==0){
        printf("Game finish!\n");
        exit(EXIT_FAILURE);
    }
    
    pthread_exit(0);
}



void *Count_msg(void *arg){

/**
 * Send penalty to the clients which failed to send messages during 8s
 **/
    buf8 = calloc(1024, sizeof(char));
    struct message ms = *((struct message *)arg);
    if(rStart==1){                                                      
        
        printf("clientfd is %i\n", ms.clientfd);
        buf8[0] = '\0';
        sprintf(buf8, "YOU ARE LATE");
        pthread_mutex_lock(&mutex);
        send(ms.clientfd, buf8, strlen(buf8),0);
        live[ms.clientfd]--;                                           //minus 1 life when a player is timeout
        pthread_mutex_unlock(&mutex);

        if(msgCount==playerCount){
        pthread_cond_broadcast(&cond1);
        }
   
        if(live[ms.clientfd]==0){                                 
            
            buf8[0] = '\0';
            sprintf(buf8, "%i,ELIM", id[ms.clientfd]);
            
            sendtoown(buf8, ms.clientfd);
            
        }
    
    }
  
    
    return (0);
}


/**
 * Function to play the game each round.
 **/

void playGameRound(int curr, char *act){
    int index;
    char *buf3 = calloc(1024, sizeof(char));
    
    pthread_mutex_lock(&mutex);
    
    index =curr;                                                 //get particular client_fd as index number
    srand (time(NULL));
    
    pthread_mutex_unlock(&mutex);
    printf("dice1: %i, dice2: %i\n", dice1, dice2);
    if(live[index]>0){
        
        printf("ID %i, live is: %i\n", id[index],live[index]);
        

        if(strstr(act, "C")!=NULL){                              // "C" means CON

            if(dice1==conn[index]||dice2==conn[index]){
   
                buf3[0] = '\0';
                sprintf(buf3, "%i,PASS", id[index]);
                sendtoown(buf3, curr);
            }
            else{
                
                live[index]--;
                
                if(live[index]==0){
                    pthread_mutex_lock(&mutex);
                    winner++;                                   //count the number of client's live reduce to zero.
                    pthread_mutex_unlock(&mutex);
                    buf3[0] = '\0';
                    sprintf(buf3, "%i,ELIM", id[index]);
                    sendtoown(buf3, curr);
                    
                }
                else{
                    
                    buf3[0] = '\0';
                    sprintf(buf3, "%i,FAIL", id[index]);
                    sendtoown(buf3, curr);
                }
            }
            conn[index] = 0;                                    //initialize the conn array element each round
        }
        else if(sum%2==0){
            
            if((dice1!=dice2)&&(strcmp(act, "E")==0)){         // "E" means EVEN
                buf3[0] = '\0';
                sprintf(buf3, "%i,PASS", id[index]);
                sendtoown(buf3, curr);
            }
            else if((dice1==dice2)&&(strcmp(act,"D")==0)){     //"D" means DOUB
                buf3[0] = '\0';
                sprintf(buf3, "%i,PASS", id[index]);
                sendtoown(buf3, curr);
                
            }
            else{
                
                live[index]--;
                if(live[index]==0){
                    pthread_mutex_lock(&mutex);
                    winner++;
                    pthread_mutex_unlock(&mutex);
                    buf3[0] = '\0';
                    sprintf(buf3, "%i,ELIM", id[index]);
                    sendtoown(buf3, curr);
                    
                }
                else{
                    
                    buf3[0] = '\0';
                    sprintf(buf3, "%i,FAIL", id[index]);
                    sendtoown(buf3, curr);
                }
            }
        }
        else{ 
            if((sum>5)&&(strcmp(act,"O")==0)){               //"O" means ODD
                buf3[0] = '\0';
                sprintf(buf3, "%i,PASS", id[index]);
                sendtoown(buf3, curr);
            }
            else{
                
                live[index]--;
                if(live[index]==0){
                    pthread_mutex_lock(&mutex);
                    winner++;
                    pthread_mutex_unlock(&mutex);
                    buf3[0] = '\0';
                    sprintf(buf3, "%i,ELIM", id[index]);
                    sendtoown(buf3, curr);
                    
                }
                else{
                    
                    buf3[0] = '\0';
                    sprintf(buf3, "%i,FAIL", id[index]);
                    sendtoown(buf3, curr);
                }
            }
        }
        pthread_mutex_lock(&mutex);                                         
        
        if(msgCount==playerCount||msgCount>playerCount){                 //initialize msgCount & rStart to 0
            msgCount=0;
            rStart = 0;
        }
        
        
        if(msgCount==0){
            pthread_create(&rClock,NULL,roundClock,NULL);                //start the timer after each round ends
        }
        pthread_mutex_unlock(&mutex);
        
    }
    if(live[index]==0){
        pthread_mutex_lock(&mutex);
        winner++;
        pthread_mutex_unlock(&mutex);
        buf3[0] = '\0';
        sprintf(buf3, "%i,ELIM", id[index]);
        sendtoown(buf3, curr);
    }
 
}


/**
 * Function to split the incoming message in each round
 **/

void * splitmsg(void * info){

    char *buf4 = calloc(1024, sizeof(char));
    struct message ms = *((struct message *)info);  //struct is used to get the specific message and client_fd
    
    if(isalpha(ms.mess[0])){                       //make sure the first character of message is digit. otherwise its invalid
        buf4[0] = '\0';
        sprintf(buf4, "Invalid message");
        sendtoown(buf4,ms.clientfd);
        sprintf(buf4,"%i,ELIM", preid[ms.clientfd]);
        sendtoown(buf4,ms.clientfd);
        return (0);
    }
    if(strlen(ms.mess)>13){                       //check the message length is less than or equal to 14.
        buf4[0] = '\0';
        sprintf(buf4, "Invalid message");
        sendtoown(buf4,ms.clientfd);
        sprintf(buf4,"%i,ELIM", preid[ms.clientfd]);
        sendtoown(buf4,ms.clientfd);
        return (0);
    }
    if(strstr(ms.mess, ",")==NULL){               //check if the message contains comma.
        buf4[0] = '\0';
        sprintf(buf4, "Invalid message");
        sendtoown(buf4,ms.clientfd);
        sprintf(buf4,"%i,ELIM", preid[ms.clientfd]);
        sendtoown(buf4,ms.clientfd);
        return (0);
    }
    bool idflag = true;
    
    const char s[2] = ",";
    char *token;
    
    token = strtok(ms.mess, s);
    while( token != NULL ) {
        if(isdigit(*token)){
            
            if(idflag){
                tempid1 = atoi(token);
                digitcount = log10(tempid1)+1;
                if(digitcount!=3){               //make sure the id has 3 digits
                    printf("invalid id\n");
                    buf4[0] = '\0';
                    sprintf(buf4,"%i,ELIM", preid[ms.clientfd]);
                    sendtoown(buf4,ms.clientfd);
                    idflag = false;
                    return(0);
                }
                
                
                else{
                    id[ms.clientfd] = tempid1;   //store id in id array, use client_fd as index number
                    
                    idflag = false;
                    printf("id is %i\n", id[ms.clientfd]);
                    
                    
                }
            }
            else{
                int contain = atoi(token);
                if(contain>6||contain<1){      //check if contain number is out of range
                    buf4[0] = '\0';
                    sprintf(buf4, "invalid, live -1");
                    sendtoown(buf4,ms.clientfd);
                    live[ms.clientfd]--;
                }else{
                    conn[ms.clientfd] = contain;   //store contain number in conn array
                    
                    
                }
            }
        }
        
        if((strcmp(token, "EVEN")==0)||(strcmp(token, "DOUB")==0)||(strcmp(token, "CON")==0)||(strcmp(token, "ODD")==0)){
            char a = *token;
            action[ms.clientfd] = &a;          //store action character in array
        }
        
        
        
        
        token = strtok(NULL, s);
    }
    
    idflag = true;
    
    pthread_mutex_lock(&mutex);
    
    //roll the dice
    
    srand (time(NULL));
    dice1 = rand() % 6 + 1;
    dice2 = rand() % 6 + 1;
    sum = dice1+dice2;
    
    pthread_mutex_unlock(&mutex);
    
    //enter the game
    playGameRound(ms.clientfd, action[ms.clientfd]);
    return (0);
    
}



/**
 * Dealing with the message recived by the clients
 **/

void *recvmg(void *sock)
{
    struct client_info cl = *((struct client_info *)sock);
    struct message ms;
    char msg[500];
    int len;
    int i;
    int j;
    int k;
    int num;
    char *buf = calloc(1024, sizeof(char));
    char *buf2 = calloc(1024, sizeof(char));

    pthread_mutex_lock(&mutex);
    if(msgCount==0&&gaming==false){
        
        pthread_create(&rClock,NULL,roundClock,NULL);               //start a timer once before any message recvied ,it will only runs once
    }
    pthread_mutex_unlock(&mutex);
    
    while((len = recv(cl.sockno,msg,500,0)) > 0) {
        if(strstr(msg, ",")==NULL&&strcmp(msg, "INIT")!=0){
            //check if a incoming message is invalid
            buf[0] = '\0';
            sprintf(buf, "%i,ELIM",preid[cl.sockno]);
            sendtoown(buf,cl.sockno);
            break;
        }
        if((strstr(msg, "MOV")==NULL)&&(strstr(msg, "INIT")==NULL)){
            //check if a incoming message is invalid
            buf[0] = '\0';
            sprintf(buf, "invalid message, ELIM");
            sendtoown(buf,cl.sockno);
            break;
        }
        
        if(strstr(msg, "INIT")!=NULL){
            
          
            
            if(gaming==true){                                      //reject a clinet if game already start
                pthread_mutex_lock(&mutex);
                lateplayer++;                                      //count the player connected after the game start
                pthread_mutex_unlock(&mutex);
                buf[0]='\0';
                sprintf(buf,"you are late!!");
                sendtoown(buf,cl.sockno);
                buf[0] = '\0';
                sprintf(buf,"REJECT");
                sendtoown(buf,cl.sockno);
                break;
            }
            pthread_mutex_lock(&mutex);
            playerCount++;
            pthread_mutex_unlock(&mutex);
            
            preid = malloc(sizeof(int)*100);                       //initialize an id array and assign an id for each clients
            for(int y = 0; y<100; y++){
                preid[y] = initialID;
                pthread_mutex_lock(&mutex);
                initialID++;
                pthread_mutex_unlock(&mutex);
            }
            
            buf[0] = '\0';
            sprintf(buf,"WELCOME,%i", preid[cl.sockno]);
            pthread_mutex_lock(&mutex);
            
            pthread_mutex_unlock(&mutex);
            sendtoown(buf,cl.sockno);
            
            
            
            if(playerCount==maxplayer&&gaming==false){          //if enough player, start the game
                live = malloc(sizeof(int)*1000);                //initialize live array and give a default value for live
                for(int t = 0; t<1000; t++){
                    live[t] = 5;                                //each client has 5 lives
                }
                
                buf[0]= '\0';
                sprintf(buf,"START,%i,%i", playerCount, live[cl.sockno]);
                pthread_mutex_lock(&mutex);
                gaming = true;
                pthread_mutex_unlock(&mutex);
                sendtoall(buf,cl.sockno);
            }
            
        }
        
        
        printf("reveiced %s\n", msg);

        if(strstr(msg, "MOV")!=NULL){
        
            //if message contain MOV
            
            pthread_mutex_lock(&mutex);
            msgCount++;                                     //Counts the incoming messeages      
            
            ms.clientfd = cl.sockno;
            ms.mess = msg;

            pthread_create(&cmsg,NULL,Count_msg,&ms);       
           
           /*continue the game when the penalty is delivered, if there are no clients obey the rules, round will start immediately */
            pthread_cond_wait(&cond1,&mutex);
            
            pthread_create(&splitt, NULL, splitmsg, &ms);
            sleep(1);                                      //sleep 1 seconds to make sure the index number will not clash
            pthread_mutex_unlock(&mutex);
        }
        
        
        
        
        
        
        memset(msg,'\0',sizeof(msg));
        
    }
    
    
    //if no message received, disconnected a client
    pthread_mutex_lock(&mutex);
    printf("%d disconnected\n",cl.sockno);
    
    if(lateplayer==0){                                   //make sure the late player is zero before minus playerCount
        playerCount--;
    }
    else{
        lateplayer--;
    }
    
    if(playerCount<pcount&&gaming==true&&msgCount==playerCount){
        pthread_cond_broadcast(&cond1);
    }
    
    
    //for loop to re-allocate the client array
    for(i = 0; i < n; i++) {
        if(clients[i] == cl.sockno) {
            j = i;
            while(j < n-1) {
                clients[j] = clients[j+1];
                j++;
            }
        }
    }
    n--;
    x--;
    
    if(playerCount==1&&gaming==true){
        if(maxplayer==1){
            printf(" ");
        }
        else{                                     //send VICT to last survive player
            buf[0] = '\0';
            sprintf(buf, "%i,VICT", preid[cl.sockno]);
            
            send(clients[0],buf, strlen(buf), 0);
        }
    }
    
    if(n==0){                                       //if no clients connected
        printf("Game finish!\n");
        exit(EXIT_FAILURE);
    }
    
    pthread_mutex_unlock(&mutex);
    
    if(playerCount==1&&gaming==true){
        buf[0] = '\0';
        sprintf(buf, "%i,VICT", preid[cl.sockno]);
        sendtoown(buf,clients[n]);
    }
    if(n==0){                                         //if no clients connected
        printf("Game finish!\n");
        exit(EXIT_FAILURE);
    }
    return (0);
    
}

int main(int argc,char *argv[])
{
    
    struct sockaddr_in server,client;
    int server_fd;
    int client_fd;
    int opt_val;
    int err;
    socklen_t client_fd_size;
    int port;
    pthread_t sendt,recvt,count;       
    char msg[500];
    int len;
    struct client_info cl;
    char ip[INET_ADDRSTRLEN];;
    ;
    if(argc > 2) {
        printf("too many arguments");
        exit(1);
    }
    
    /**
     * you can define how many player are allow in each game
     **/
    
    printf("Please enter the number of player in this game: \n");
    scanf("%d", &maxplayer);
    while(maxplayer<1||maxplayer>7){
        printf("Enter a number between 1 and 8\n");
        scanf("%d", &maxplayer);
    }
    port = atoi(argv[1]);
    server_fd = socket(AF_INET,SOCK_STREAM,0);
    if (server_fd < 0){
        fprintf(stderr,"Could not create socket\n");
        exit(EXIT_FAILURE);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    client_fd_size = sizeof(client);
    opt_val=1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);
    
    err = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
    if (err < 0){
        fprintf(stderr,"Could not bind socket\n");
        exit(EXIT_FAILURE);
    }
    
    err = listen(server_fd, 128); 
    if (err < 0){
        fprintf(stderr,"Could not listen on socket\n");
        exit(EXIT_FAILURE);
    }
    printf("Server is listening on %d\n", port);
    
    while(1) {
        pthread_mutex_lock(&mutex);
                                                                   //start the timer, wait 20 seconds for players to join
        if(n==0){
            
            pthread_create(&count,NULL,countdown,NULL);
        }
        pthread_mutex_unlock(&mutex);
        
        if((client_fd = accept(server_fd,(struct sockaddr *)&client,&client_fd_size)) < 0) {
            perror("accept unsuccessful");
            exit(1);
        }
        pthread_mutex_lock(&mutex);
        inet_ntop(AF_INET, (struct sockaddr *)&client, ip, INET_ADDRSTRLEN);
        printf("%s connected\n",ip);
                                                                    //put client_fd into array
        cl.sockno = client_fd;
        strcpy(cl.ip,ip);
        clients[n] = client_fd;
        n++;
        printf("client_fd is: %i\n", client_fd);
        pthread_create(&recvt,NULL,recvmg,&cl);
        
        pthread_mutex_unlock(&mutex);
        
    }
    
    return 0;
}

