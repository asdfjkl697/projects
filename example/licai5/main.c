#include<stdio.h>
#include<math.h>

#define LIFE       80
#define STARTYEAR  30
#define ALLYEAR    (LIFE-STARTYEAR)
#define money 	   20000

#define Insurance  300
#define interest   1.04

#define zongbaofei 10
#define zhushoujin 1
#define meinianfanhuan 0.15

int main()
{
	int i,year=0;
	//float money_y=10000.0;
	float tyear=20000;
	float total=0;
	float money_y;
	
	for(i=1;i<11;i++)
	{
		tyear=money*pow(interest,(ALLYEAR-i));
		total+=tyear;
	}
	printf("select0: %f\n",total);
	
	tyear=0;
	for(i=0;i<ALLYEAR;i++)
	{
		tyear=(tyear-Insurance-money*meinianfanhuan)*interest;
		if(i<10)
		tyear+=10000;
	}
	printf("select1: %f\n",tyear);

	money_y=money*(ALLYEAR*meinianfanhuan+zongbaofei*pow(interest,20)+zhushoujin);
	printf("select2: %f\n",money_y);
	return 0;
}
