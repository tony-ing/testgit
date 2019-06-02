/* Student name & sutdent ID
 * Xuelin Wang 21820158
 * Wencheng Xiong 21782296
 * our server can run on the MAC and we defined the port and do not need to input the port number, the port number is 4444
 */


#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define MAX_PLAYER_NUM 6/*max value is 32*/
#define MIN_PLAYER_NUM 4
#define MAX_LIFE_NUM 8/*max life num*/
/*server port define*/
#define MYPORT 4444
/*show debug information OR not*/
#define HTRACE printf
#define DTRACE printf
/*roll dice and generate a value that contains all correct results*/
static unsigned short diceRoll(void)
{
	unsigned char DiceValue[2]={0};
	unsigned short sumValue;
	/*random dice generate*/
	DiceValue[0]=rand()%6;/*from 0--5*/
	DiceValue[1]=rand()%6;/*from 0--5*/
	/*calculate*/
	sumValue=DiceValue[0]+DiceValue[1]+2;/*[1,6]+[1,6]*/
	if(sumValue&0x01)/*ODD ,obviously >=3*/
	{
		if(sumValue>5)
		{
			sumValue=0x80;
		}
	}
	else/*EVEN*/
	{
		sumValue=0x40;
	}
	/*check double*/
	if(DiceValue[0]==DiceValue[1])
	{
		sumValue|=0x100;
	}
	/*check contain first dice*/
	sumValue|=(1<<DiceValue[0]);
	/*check contain second dice*/
	sumValue|=(1<<DiceValue[1]);
	/*show me for debug*/
	DTRACE("Dices=%d & %d[%03x]\n",DiceValue[0]+1,DiceValue[1]+1,sumValue);
	return sumValue;
}
/*decode player guess to value*/
static unsigned short decodeMessage(char* str,int len)
{
	unsigned short IDs=0;
	while((*str==' ')&&(len))
	{
		str++;
		len--;
	}
	if(len==0)/*message err*/
	{
		return 0xFFFF;
	}

	if(strcmp(str,"INIT")==0)
	{
		return 0;
	}
	else/*try to find ','*/
	{
		char *dot=str,n=0;
		if((*dot<'0')||(*dot>'9'))
		{
			return 0xFFFF;
		}
		while((*dot>='0')&&(*dot<='9')&&(n<3))
		{
			IDs*=10;
			IDs+=(*dot-'0');
			n++;
			dot++;
		}
		if((n==0)&&(*dot!=','))/*ID not found OR not DOT*/
		{
			return 0xFFFF;
		}
		/*Guess type*/
		if(strncmp(dot,",MOV,EVEN",9)==0)
		{
			IDs|=0x7000;
		}
		else if(strncmp(dot,",MOV,ODD",8)==0)
		{
			IDs|=0x8000;
		}
		else if(strncmp(dot,",MOV,DOUB",9)==0)
		{
			IDs|=0x9000;
		}
		else if(strncmp(dot,",MOV,CON,",9)==0)
		{
			dot+=9;/*should be number now*/
			if((*dot>='0')&&(*dot<='9'))/*only check one number*///if((*dot>='0')&&(*dot<='9')&&(*(dot+1)>='1')&&(*(dot+1)<='9'))
			{
				n=(*dot-'0');
				//n*=10;/*only check one number*/
				//n+=(*(dot+1)-'0');/*only check one number*/
				if((n<=6)&&(n>=1))
				{
					IDs|=(n<<12);
				}
				else
				{
					IDs|=0xA000;/*cheater*/
				}
			}
			else
			{
				IDs=0xFFFF;
			}
		}
		else
		{
			IDs=0xFFFF;
		}
	}
	/*
	bit0--bit11: client ID
	bit12-bit16:	0     INIT
					1-6	  MOV,CON,1-6
					7	  MOV,EVEN
					8	  MOV,ODD
					9	  MOV,DOUB
					A	  Cheater
					F	  NULL
	*/
	return IDs;
}

/*semaphore union define*/
//union semun{
	//int val;
//};
/*semaphore ID deine*/
static int semid=0;

static char sem_init(void)
{
	/*assume key 1234 is available, creat the semaphore*/
	semid=semget((key_t)1234,1,IPC_CREAT|0666);
	if(semid==-1)/*should't*/
	{
		HTRACE("semaphore creat err\n");
		return 1;
	}
	else
	{
		union semun v;
		v.val=1;
		if(semctl(semid,0,SETVAL,v)==-1)
		{
			HTRACE("semaphore ctrl init err\n");
			return 1;
		}	
	}
	return 0;
}
static char semOperate(int val)
{
	struct sembuf buf;
	buf.sem_num=0;
	buf.sem_op=val;/*-1 means P operation & 1 means V operation*/
	buf.sem_flg=SEM_UNDO;
	if(semop(semid,&buf,1)==-1)
	{
		HTRACE("semctrl operate err\n");
		return 1;
	}
	return 0;
}
static char sem_destory(void)
{
	if(semop(semid,0,IPC_RMID)==-1)
	{
		//HTRACE("semctrl destory err\n");/*whatever, gameover*/
		return 1;
	}
	return 0;	
}
/*share memory*/
typedef struct{
	/*player info*/
	unsigned char playerCount;/*current active player num*/
	unsigned char playerState[MAX_PLAYER_NUM];/* 0 no socket; 1 wait init; 2 wait start; 3 wait receive; 4 wait new round; 5 cancle ;else exit*/
	unsigned short playerResult[MAX_PLAYER_NUM];/* 0 no answer, else answer result*/
	unsigned char playerLife[MAX_PLAYER_NUM];
	unsigned char playerAckType[MAX_PLAYER_NUM];
	unsigned short playerRcvDoneFlag;
	/*game info*/
	unsigned short gameState;/*state of current game; 0 wait start; 1 start one round; 2 wait result; 3 cancel ; 4 exit*/
	unsigned int timer;/*time elapse*/
}SHM_DEF;
static int shmid;
static char shm_init(char** addr)
{
	char i;
	char* shm_addr;
	shmid=shmget((key_t)4321,sizeof(SHM_DEF),IPC_CREAT | 0666);

	if(shmid == -1)
	{
		HTRACE("cannot creat a share memory\n");
		return 1;
	}
	 
	shm_addr=(char*)shmat(shmid,NULL,0);
	if(shm_addr==(char*)(-1))
	{
		HTRACE("cannot attach the shared memory to process");
		return 1;
	}
	*addr=shm_addr;
	return 0;
}
static char shm_destory(char* addr)
{
	if(shmdt(addr)==-1)
	{
		HTRACE("cannot release the memory");
		return 1;
	}

	if(shmctl(shmid,IPC_RMID,NULL)==-1)
	{
		HTRACE("cannot delete existing shared memory segment");
		return 1;
	}
	return 0;
}
/*marco define*/
/*semaphore define, maybe we can use another method*/
#define SEM_OP(x) semOperate(x)
#define SEM_P SEM_OP(-1)
#define SEM_V SEM_OP(1)
#define SEM_INIT sem_init()
#define SEM_DEL sem_destory()
/*share memory define,maybe we can use another method*/
#define SHM_INIT(a) shm_init(&a)
#define SHM_DEL(a) shm_destory(a)

/*logical check define*/
typedef enum{
	GAME_STATE_WAIT_JOIN=0,
	GAME_STATE_ROLL_ONE,
	GAME_STATE_WAIT_GEUSS,
	GAME_STATE_CANCLE,
	GAME_STATE_OVER,
}GAME_STATE;
typedef enum{
	PLAYER_STATE_IDEL=0,
	PLAYER_STATE_INIT,
	PLAYER_STATE_START,
	PLAYER_STATE_RECEIVE,
	PLAYER_STATE_CHECK_RESULT,
	PLAYER_STATE_ACK,
	PLAYER_STATE_DONE,
	PLAYER_STATE_CANCEL,
	PLAYER_STATE_OVER,
}PLAYER_STATE;
typedef enum{
	ACK_RESULT_NULL=0,
	ACK_RESULT_VICT,
	ACK_RESULT_ELIM,
	ACK_RESULT_PASS,
	ACK_RESULT_FAIL,
}RESULT_TYPE;
#define CURRENT_GAME_STATE (Shm->gameState)/*current state*/
#define CURRENT_PLAYER_NUM (Shm->playerCount)

#define CURRENT_TIME_ELAPSE (Shm->timer)
#define CURRENT_PLAYER_STATE(index) (Shm->playerState[index])
#define DO_INCREASE_TIMER (Shm->timer++)
#define DO_DISABLE_NEW_PLAYER_JOIN  (Shm->timer=31)/*stop new player join*/

#define IS_OK_TO_STOP_LISTEN(state) ((state==GAME_STATE_OVER)||(state==GAME_STATE_CANCLE))/*game over OR cancel*/
#define IS_ABLE_TO_ACCEPT_NEW_PLAYER(num,times) ((num<MIN_PLAYER_NUM)||(times<30))
#define IS_OK_TO_CANCEL(times) (times>=30) 

#define PLAYER_STATE_SWITCH(index,newState) Shm->playerState[index]=newState;
#define PLAYER_RESULT_SET(index,result)	Shm->playerResult[index]=result;

#define GAME_STATE_SWITCH(state) Shm->gameState=state;
/*
	description
	1. the main idea is multi-process + shared memory + semaphore
	2. a banker child process to play the game,
	2. every time after rolling dices, ervery correct answer is set as bit in the Result indicate by a unsigned short value
	   so it's easy to check if a player given a correct answer just by 'AND'
*/
int main(int argc,char *argv[ ])
{
	int sock_fd, new_fd,maxsock;  // listen on sock_fd, new connection on new_fd, etc.
    struct sockaddr_in server_addr;    // server(banker) address information
    struct sockaddr_in client_addr; // client(player) address information
    socklen_t sin_size;
    int ret,yes=1,delayCounter=0,clientCount=0;
	fd_set fdsr;
    struct timeval tv;
	pid_t pid,pidMaster;/*fork child process*/
	char* shmShadow=NULL;
	SHM_DEF* Shm;
	/*shared memory creat*/
	srand((unsigned)time(NULL));
	if(SHM_INIT(shmShadow))
	{
		HTRACE("shared memory failed\n");
		return 0;
	}
	else
	{
		printf("Shm Addr=%d\n",(int)shmShadow);
		/*clear shared memory to zero first*/
		for(ret=0;ret<sizeof(SHM_DEF);ret++)
		{
			*shmShadow=0;
		}
		/*now i can use the shared memory as expected*/
		Shm=(SHM_DEF*)shmShadow;
	}
	/*semaphore creat*/
	if(SEM_INIT)
	{
		HTRACE("semaphore failed\n");
		return 0;
	}
	/*creat a socket*/
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
        HTRACE("socket failed\n");
        return 0;
    }
 
    if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{
        HTRACE("setsockopt failed\n");
        return 0;
    }

    server_addr.sin_family = AF_INET;         // host byte order
    server_addr.sin_port = htons(MYPORT);     // short, network byte order
    server_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
    memset(server_addr.sin_zero, 0, sizeof(server_addr.sin_zero));
	/*bind the socket*/
    if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
	{
        HTRACE("bind failed\n");
        return 0;
    }
	/*listen the connection request*/
    if (listen(sock_fd, 15) == -1) 
	{
        HTRACE("listen failed\n");
        return 0;
    }
    
    sin_size = sizeof(client_addr);
    maxsock = sock_fd;
	/*to play the game while listening new connection request ,we need a child process named banker to play the game*/
	pidMaster=fork();
	if(pidMaster==-1)/*creat banker process failed*/
	{
		HTRACE("can't creat Master process\n");
		return -1;
	}
	else if(pidMaster==0)/*child process , roll dices & decide when the game start,end,etc*/
	{
		char bankerState=0,exitGame=0,i;
		char Pnum,Tcount;
		unsigned int sendFailedFlag=0,roundCount=0;
		unsigned short diceResult;
		int selectIndex,sendRes;
		DTRACE("Banker Process Start\n");
		while(1)
		{
			SEM_P;
			bankerState=CURRENT_GAME_STATE;
			SEM_V;
			switch(bankerState)/*state of current game; 0 wait start; 1 start one round; 2 wait result; 3 cancel ; 4 exit*/
			{
				case GAME_STATE_WAIT_JOIN:/*wait player join*/
					Pnum=0;
					SEM_P;
					for(i=0;i<MAX_PLAYER_NUM;i++)
					{
						if(Shm->playerState[i]==PLAYER_STATE_INIT)/*this is waiting start*/
						{
							Pnum++;
						}
					}
					Tcount=CURRENT_TIME_ELAPSE;
					SEM_V;
					/*enough player to satrt the game*/
					if(Pnum>=MIN_PLAYER_NUM)
					{
						char msg[14]={0};
						DTRACE("game start,player count=%d\n",Pnum);
						sprintf(msg,"START,%02d,%02d\0",Pnum,MAX_LIFE_NUM);
						SEM_P;
						Shm->playerRcvDoneFlag=0;
						DO_DISABLE_NEW_PLAYER_JOIN;
						for(i=0;i<MAX_PLAYER_NUM;i++)
						{
							if(Shm->playerState[i]==PLAYER_STATE_INIT)
							{
								Shm->playerLife[i]=MAX_LIFE_NUM;
								Shm->playerRcvDoneFlag|=(1<<i);
								PLAYER_STATE_SWITCH(i,PLAYER_STATE_START);/*ack start message*/
							}
						}
						Shm->playerCount=Pnum;
						SEM_V;
						GAME_STATE_SWITCH(GAME_STATE_ROLL_ONE);/*wait result*/
					}
					else if(IS_OK_TO_CANCEL(Tcount))/*timeout, not enough player, cancel*/
					{
						DTRACE("cancel game[%d],player count=%d\n",Tcount,Pnum);
						SEM_P;
						for(i=0;i<MAX_PLAYER_NUM;i++)
						{
							if(Shm->playerState[i]!=PLAYER_STATE_IDEL)
							{
								PLAYER_STATE_SWITCH(i,PLAYER_STATE_CANCEL);/*ack cancel message*/
							}
						}
						SEM_V;
						GAME_STATE_SWITCH(GAME_STATE_CANCLE);/*cancel*/
					}
					else
					{
						sleep(1);
					}
					break;
				case GAME_STATE_ROLL_ONE:/*never*/
				{
					/*all player should answer OR lost one life*/
					unsigned short crtState;
					Pnum=0;
					
					//for(i=0;i<MAX_PLAYER_NUM;i++)
					//{
					//	SEM_P;
					//	crtState=Shm->playerState[i];
					//	SEM_V;
					//	if((crtState==PLAYER_STATE_START)||(crtState==PLAYER_STATE_ACK))
					//	{
					//		Pnum++;/*no answer*/
					//	}
					//}
					SEM_P;
					Pnum=Shm->playerRcvDoneFlag;
					SEM_V;
					if(Pnum==0)
					{
						SEM_P;
						GAME_STATE_SWITCH(GAME_STATE_WAIT_GEUSS);/*wait result*/
						SEM_V;
						DTRACE("All receive done & Round [%d] then\n",++roundCount+1);
					}
					else
					{
						DTRACE("wait\n");
						sleep(1);
					}
					
					break;
				}
				case GAME_STATE_WAIT_GEUSS:/*wait result*/
				{
					unsigned short ans,msk,passFlag=0,loseFlag=0,elimFlag=0,winner=0;
					char passCount=0,loseCount=0,elimCount=0,types=0;
					/*check left player num*/
					Pnum=0;
					diceResult=diceRoll();
					SEM_P;
					for(i=0;i<MAX_PLAYER_NUM;i++)
					{
						if(Shm->playerLife[i]>0)
						{
							ans=Shm->playerResult[i];
							
							msk=(1<<(((ans>>12)&0xFF)-1));
DTRACE("%d[%d]:result:%d,%d[%03x]\n",100+i,Shm->playerLife[i],ans&0xFF,(ans>>12)&0xFF,msk);
							if(((ans&0xFFF)!=(100+i))||((ans&0xF000)==0xA000))/*cheater-elim*/
							{
								Shm->playerLife[i]=0;
								elimFlag|=(1<<i);
								elimCount++;
							}
							else if(diceResult&msk)/*pass*/
							{
								Pnum++;
								passCount++;
								passFlag|=(1<<i);
							}
							else/*fail*/
							{
								Shm->playerLife[i]--;
								if(Shm->playerLife[i]==0)/*elim*/
								{
									elimFlag|=(1<<i);
									elimCount++;
								}
								else/*fail*/
								{
									loseFlag|=(1<<i);
									Pnum++;
									loseCount++;
								}
							}
						}								
					}
					Shm->playerRcvDoneFlag=0;
					SEM_V;
					/*send message*/
					if(Pnum==1)/*game over,left winner*/
					{
						if(passCount>loseCount)/*winner is who pass this round*/
						{
							winner=passFlag;
							passFlag=0;
						}
						else/*winner is who lose this round*/
						{
							winner=loseFlag;
							loseFlag=0;
						}
						exitGame=1;/*game over*/
					}
					/*only two left and they draw*/
					if((elimCount==2)&&(Pnum==0))
					{
						winner=elimFlag;/*draw, two winners*/
						exitGame=1;/*game over*/
					}
					if(Pnum==0)/*nobody left, game over*/
					{
						exitGame=1;/*game over*/
					}
					msk=1;
					for(i=0;i<MAX_PLAYER_NUM;i++)
					{
						ans=1;
						if(winner&msk)/*winner--'VICT'*/
						{
							types=ACK_RESULT_VICT;					
						}
						else if(passFlag&msk)/*'PASS'*/
						{
							types=ACK_RESULT_PASS;
						}
						else if(loseFlag&msk)/*'FAIL'*/
						{
							types=ACK_RESULT_FAIL;
						}
						else if(elimFlag&msk)/*'ELIM'*/
						{
							types=ACK_RESULT_ELIM;
						}
						else/*no use*/
						{
							types=ACK_RESULT_NULL;
							ans=0;
						}
						
						if(ans)
						{
							SEM_P;
							if((types==ACK_RESULT_PASS)||(types==ACK_RESULT_FAIL))
							{
								Shm->playerRcvDoneFlag|=(1<<i);
							}
							Shm->playerAckType[i]=types;
							PLAYER_STATE_SWITCH(i,PLAYER_STATE_ACK);/*ack message*/
							Shm->playerResult[i]=0;
							SEM_V;
							DTRACE("%d will ack[%c]\n",100+i,"XVEPF"[types]);
						}
						msk<<=1;
					}
					/*next round*/
					GAME_STATE_SWITCH(GAME_STATE_ROLL_ONE);/*wait result*/
				}
				break;
				case GAME_STATE_CANCLE:/*wait result*/
					sleep(1);
				case GAME_STATE_OVER:
				default:
					exitGame=1;
					break;
			}
			
			if(exitGame)
			{
				sleep(1);
				break;
			}
		}
		/*let's all player close socket*/
		SEM_P;
		for(i=0;i<MAX_PLAYER_NUM;i++)
		{
			if(Shm->playerState[i]!=0)
			{
				/*let's stop socket*/
				PLAYER_STATE_SWITCH(i,PLAYER_STATE_OVER);
			}
		}
		SEM_V;
		sleep(1);
		/*just in case*/
		SEM_P;
		for(i=0;i<MAX_PLAYER_NUM;i++)
		{
			if(Shm->playerState[i]!=PLAYER_STATE_IDEL)
			{
				PLAYER_STATE_SWITCH(i,PLAYER_STATE_OVER);/*let's stop socket*/
			}
		}

		GAME_STATE_SWITCH(GAME_STATE_OVER);/*game ove*/
		SEM_V;
		/*exit process*/
		exit(0);
	}
	/*main process will keep listening new connection request even during the game, cause we need 'REJECT' it*/
	DTRACE("listen port %d\n", MYPORT);
	/*try to connect every connection request*/
	while (1)/**/
	{
		char curtState;
        // initialize file descriptor set
        FD_ZERO(&fdsr);
        FD_SET(sock_fd, &fdsr);
        // timeout setting, try every second until game over
        tv.tv_sec = 1;
        tv.tv_usec = 0;
		/*if the banker process decide to end the game, stop listening, Or keep listening*/
		SEM_P;
		DO_INCREASE_TIMER;
		curtState=CURRENT_GAME_STATE;
		SEM_V;
		if(IS_OK_TO_STOP_LISTEN(curtState))
		{
			break;/*stop listening*/
		}
		/*try once*/
        ret = select(maxsock + 1, &fdsr, NULL, NULL, &tv);
		/*increase try counter,which also means time elapse*/
		
		/*check select result*/
        if (ret < 0) /*err occur*/
		{
            DTRACE("select err\n");
            continue;
        } 
		else if (ret == 0)/*timeout this time*/
		{
           // DTRACE("select timeout %d\n",delayCounter);
            continue;
        }
		else/*process a new connection request*/
		{
			if(FD_ISSET(sock_fd,&fdsr))/*really is*/
			{
				new_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &sin_size);/*accept this request first*/
				if (new_fd <= 0) /*failed, just continue*/
				{
					DTRACE("accept err\n");
				}
				else/*accept connection successfully*/
				{
					/*now fork a new child process to keep commuinicating during the game*/
					pid=fork();
					if(pid==-1)/*child process creat failed*/
					{
						DTRACE("cannot creat new process\n");
						return -1;
					}
					else if(pid==0)/*child process creat*/
					{
						char myState=0,gameState,NeedEnd=0;
						char gvIndex=-1;/*wait 'INIT'*/
						struct timeval tmout;
						extern int errno;
						//int on=1;
						//FILE *rstream,*wstream;
						//close(sock_fd);
						tmout.tv_sec = 5;/*receive timeout --2 seconds*/
						tmout.tv_usec = 0;
						if(setsockopt(new_fd, SOL_SOCKET,SO_RCVTIMEO,(char *)&tmout,sizeof(struct timeval)) == -1)
						{
							HTRACE("setsockopt rcv timeout failed\n");
							close(new_fd);
							exit(0);
						}
						//if(setsockopt(new_fd, IPPROTO_TCP, TCP_NODELAY,(char *)&on,sizeof(int)) == -1)
						//{
						//	HTRACE("setsockopt no delay failed\n");
						//	close(new_fd);
						//	exit(0);
						//}
						//rstream=fdopen(new_fd,"r");
						//wstream=fdopen(new_fd,"w");
						/*keep process player message*/
						#define RECEIVE_MSG(ms,len) recv(new_fd,ms,len,0);
						#define SEND_MSG(ms,len) send(new_fd,ms,len,0)
						//#define RECEIVE_MSG(msg,len) read(rstream,msg,len);
						//#define SEND_MSG(msg,len) write(wstream,msg,len);\
													fflush(wstream);
						while(0)//(myState++<10)
						{
							char msg[20]={0},msgs[20]={0};
							sprintf(msg,"HELLO\0");
							RECEIVE_MSG(msgs,14);
							DTRACE("[%d]RCV=>%s\n",myState,msgs);
							//sleep(1);
							SEND_MSG(msg,14);
							//sleep(1);
							
						}
						while(1)
						{
							SEM_P;
							if(gvIndex>=0)
							{
								myState=CURRENT_PLAYER_STATE(gvIndex);
							}
							SEM_V;
							
							switch(myState)/* 0 no socket; 1 wait init; 2 wait start; 3 wait receive; 4 wait new round; else exit*/
							{
								case PLAYER_STATE_IDEL:/*receive, if failed 'INIT' release the socket*/
								{
									char i,msg[20]={0},tmElapse,playerNums;
									/*try receive*/
									RECEIVE_MSG(msg,20);
									if(strncmp(msg,"INIT",4)!=0)/*err*/
									{
										NeedEnd=1;
										DTRACE("Unexceptable message:%s\n",msg);
										break;
									}
									SEM_P;
									tmElapse=CURRENT_TIME_ELAPSE;
									playerNums=CURRENT_PLAYER_NUM;
									for(i=0;i<MAX_PLAYER_NUM;i++)
									{
										if(Shm->playerState[i]==0)
										{
											PLAYER_STATE_SWITCH(i,PLAYER_STATE_INIT);/*set flag that this id is not available*/
											gvIndex=i;
											break;
										}
									}
									/*current avaivable id*/
									SEM_V;
									/*check*/
									if(IS_ABLE_TO_ACCEPT_NEW_PLAYER(playerNums,tmElapse)&&(gvIndex>=0))
									{
										sprintf(msg,"%03d,WELCOME\0",100+gvIndex);/*send 'WELCOME' first*/
										SEND_MSG(msg,14);
									}
									else
									{
										SEND_MSG("REJECT\0", 14);
										NeedEnd=1;
									}
									//sleep(1);
									DTRACE("[%d][%d]accept ack:%s\n",new_fd,gvIndex,msg);
								}
								break;
								case PLAYER_STATE_INIT:/*wait start cmd here*/
									
									break;
								case PLAYER_STATE_START:/*wait start*/
								{
									char msg[14]={0},playerCounts=0;
									SEM_P;
									playerCounts=CURRENT_PLAYER_NUM;
									SEM_V;
									sprintf(msg,"START,%02d,%02d",playerCounts,MAX_LIFE_NUM);
									SEND_MSG(msg,14);
									SEM_P;
									PLAYER_STATE_SWITCH(gvIndex,PLAYER_STATE_RECEIVE);/*wait guess message now*/
									SEM_V;
									//sleep(1);
									break;
								}
								case PLAYER_STATE_RECEIVE:/*wait guess*/
								{
									char msg[20]={0};
									int res;
									unsigned short result=0xFFFF;
									res=RECEIVE_MSG(msg,20);
									result=decodeMessage(msg,14);
									
									if(res<0)/*timeout*/
									{
										result=0xC000+100+gvIndex;
										DTRACE("[%d]rcv msg timeout[%d]\n",100+gvIndex,errno);
									}
									else if(res==0)/*connect close*/
									{
										result=0xA000;
										DTRACE("[%d]connect close\n",100+gvIndex);
									}
									else
									{
										DTRACE("[%d]rcv msg[%d]:%s\n",100+gvIndex,res,msg);
									}
									SEM_P;
									PLAYER_RESULT_SET(gvIndex,result);
									Shm->playerRcvDoneFlag&=~(1<<gvIndex);
									PLAYER_STATE_SWITCH(gvIndex,PLAYER_STATE_CHECK_RESULT);/*wait banker give result to ack*/
									SEM_V;
								}
								case PLAYER_STATE_CHECK_RESULT:
									break;
								case PLAYER_STATE_ACK:
								{
									char typex,msg[14]={0};
									int res;
									SEM_P;
									typex=Shm->playerAckType[gvIndex];
									SEM_V;
									switch(typex)
									{
										case ACK_RESULT_VICT:
											sprintf(msg,"%03d,VICT",100+gvIndex);
											res=SEND_MSG(msg,14);
											NeedEnd=1;
											break;
										case ACK_RESULT_PASS:
											sprintf(msg,"%03d,PASS",100+gvIndex);
											res=SEND_MSG(msg,14);
											break;
										case ACK_RESULT_FAIL:
											sprintf(msg,"%03d,FAIL",100+gvIndex);
											res=SEND_MSG(msg,14);
											break;
										case ACK_RESULT_ELIM:
											sprintf(msg,"%03d,ELIM",100+gvIndex);
											res=SEND_MSG(msg,14);
											NeedEnd=1;
											break;
										default:
											sprintf(msg,"Err:WTF\n");
											break;
									}
									DTRACE("[%d](%d)Ack:%s\n",100+gvIndex,res,msg);
									SEM_P;
									PLAYER_STATE_SWITCH(gvIndex,PLAYER_STATE_RECEIVE);/*aussum to wait new message*/
									SEM_V;
									break;
								}
								case PLAYER_STATE_CANCEL:
								{
									char msg[14]={0};
									sprintf(msg,"CANCEL");
									SEND_MSG(msg,14);
									NeedEnd=1;
									break;
								}
								case PLAYER_STATE_OVER:
								default:
									NeedEnd=1;
									break;
							}
							
							if(NeedEnd)
							{
								break;
							}
						}
						/*close socket*/
						close(new_fd);
						SEM_P;
						if(gvIndex>=0)
						{
							PLAYER_STATE_SWITCH(gvIndex,PLAYER_STATE_IDEL);/*set flag that this id is available*/
						}
						SEM_V;
						/*the process end and exit*/
						exit(0);
					}
					else/*main process task*/
					{
						//close(new_fd);
						DTRACE("New Client try to join\n");
					}
				}
			}
		}
	}
	
	/*game ove, release erverything*/
	sleep(1);
	SHM_DEL(shmShadow);
	SEM_DEL;
	close(sock_fd);
	close(new_fd);
	return 0; 
}
