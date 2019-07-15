#include<stdio.h>
#include<stdlib.h>

int main(char *argv[], int argc){
	int i;	
	if(argc<1){
		printf("need more arguments\n");
		return -1;
	}
	for(i =0; i<5;i++){
		printf("%s\n", argv);
		
	}

	else{
		printf("print nothing");
	}
	return 0;
}


