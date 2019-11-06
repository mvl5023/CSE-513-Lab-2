#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

void sleepRandomTime(){
	srand(time(NULL));
	float duration = rand()%5+1;
	sleep(duration);
	printf("duration: %f \n", duration);
}

int main()
{
	
	sleepRandomTime();
	sleepRandomTime();
	sleepRandomTime();
	sleepRandomTime();
	sleepRandomTime();
	return 0;
}
