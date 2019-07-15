#include<stdio.h>
#include<stdlib.h>

int main(char *argv[], int argc){
	if(argc<1){
		printf("need more arguments\n");
		return -1;
	}

	printf("helloworld\n");
	return 0;
}


