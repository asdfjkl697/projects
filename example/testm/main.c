//注意要在输出结果后面加\n，不然会有格式错误，坑爹啊!!!
#include "stdio.h"
#include "stdlib.h"

#include "mytool1.h"
#include "mytool2.h"
#define PAI_C 14

char op[5]={'#','+','-','*','/',};

float cal(float x,float y,int op)
{
  switch(op)
  {
    case 1:return x+y;
    case 2:return x-y; 
    case 3: return x*y;
    case 4: return x/y;
  }
}

float calculate_model1(float i,float j,float k,float t,int op1,int op2,int op3)
{
  float r1,r2,r3;
  r1 = cal(i,j,op1);
  r2 = cal(r1,k,op2);
  r3 = cal(r2,t,op3);
  return r3;
}

float calculate_model2(float i,float j,float k,float t,int op1,int op2,int op3)
{
  float r1,r2,r3;
  r1 = cal(j,k,op2);
  r2 = cal(i,r1,op1);
  r3 = cal(r2,t,op3);
  return r3;
}

float calculate_model3(float i,float j,float k,float t,int op1,int op2,int op3)
{
  float r1,r2,r3 ;
  r1 = cal(k,t,op3);
  r2 = cal(j,r1,op2);
  r3 = cal(i,r2,op1);
  return r3;
}


float calculate_model4(float i,float j,float k,float t,int op1,int op2,int op3)
{
  float r1,r2,r3;
  r1 = cal(j,k,op2);
  r2 = cal(r1,t,op3);
  r3 = cal(i,r2,op1);
  return r3;
}

float calculate_model5(float i,float j,float k,float t,int op1,int op2,int op3)
{
  float r1,r2,r3 ;
  r1 = cal(i,j,op1);
  r2 = cal(k,t,op3);
  r3 = cal(r1,r2,op2);
  return r3;
}

int get24(int i,int j,int k,int t)
{
    int op1,op2,op3;
    int flag=0;
    for(op1=1;op1<=4;op1++)
        for(op2=1;op2<=4;op2++)
            for(op3=1;op3<=4;op3++)
            {
               if(calculate_model1(i,j,k,t,op1,op2,op3)==24){
		  printf("((%d%c%d)%c%d)%c%d\n",i,op[op1],j,op[op2],k,op[op3],t);
	          flag = 1;goto OUT;
               }
               if(calculate_model2(i,j,k,t,op1,op2,op3)==24){
		   printf("(%d%c(%d%c%d))%c%d\n",i,op[op1],j,op[op2],k,op[op3],t);
	           flag = 1;goto OUT;
	       }
               if(calculate_model3(i,j,k,t,op1,op2,op3)==24){
		   printf("%d%c(%d%c(%d%c%d))\n",i,op[op1],j,op[op2],k,op[op3],t);
	           flag = 1;goto OUT;
		}
               if(calculate_model4(i,j,k,t,op1,op2,op3)==24){
		   printf("%d%c((%d%c%d)%c%d)\n",i,op[op1],j,op[op2],k,op[op3],t);
	           flag = 1;goto OUT;
			   }
               if(calculate_model5(i,j,k,t,op1,op2,op3)==24){
		   printf("(%d%c%d)%c(%d%c%d)\n",i,op[op1],j,op[op2],k,op[op3],t);
	           flag = 1;goto OUT;
			   }
            }

OUT:    
	return flag;
}


int step_one(int *in)
{
    int x,y,m,n;
    int i,j,k,t;
    int flag;
    for(i=0;i<4;i++){
        for(j=0;j<4;j++){
            if(j==i) continue;
            for(k=0;k<4;k++){
                if(i==k||j==k) continue;
                for(t=0;t<4;t++){
                    if(t==i||t==j||t==k) continue;
                    x = in[i];
                    y = in[j];
                    m = in[k];
                    n = in[t];
                    flag = get24(x,y,m,n);
                    if(flag ==1)return 1;
                }
            }
        }
    }
    if(flag == 0)
    {
        //printf("%d %d %d %d ",in[0],in[1],in[2],in[3]);
	printf("-1\n");
    }
}

int main()
{
    
    int in[4],i,j,k,l,num=0,allnum=0,ret;
    in[0]=1;in[1]=3;in[2]=4;in[3]=4;
    for(i=1;i<PAI_C;i++)
    for(j=i;j<PAI_C;j++)
    for(k=j;k<PAI_C;k++)
    for(l=k;l<PAI_C;l++)
    {
	in[0]=i;in[1]=j;in[2]=k;in[3]=l;	
	ret=0;
	ret=step_one(in);
	if(ret==1)num++;
	allnum++;
    }
    printf("%d %d\n",num,allnum);
    mytool1_print("hello");
    mytool2_print("hello");
    return 0;
}
