//注意要在输出结果后面加\n，不然会有格式错误，坑爹啊!!!
#include "stdio.h"
#include "stdlib.h"
#define PAI_C 14
#define PAI_A 72

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

float calculate_model1(float i,float j,float k,float t,float l,int op1,int op2,int op3,int op4)
{
  float r1,r2,r3,r4;
  r1 = cal(i,j,op1);
  r2 = cal(r1,k,op2);
  r3 = cal(r2,t,op3);
  r4 = cal(r3,l,op4);
  return r4;
}

float calculate_model2(float i,float j,float k,float t,float l,int op1,int op2,int op3,int op4)
{
  float r1,r2,r3,r4;
  r1 = cal(i,j,op1);
  r2 = cal(r1,k,op2);
  r3 = cal(t,l,op4);
  r4 = cal(r2,r3,op3);
  return r4;
}

float calculate_model3(float i,float j,float k,float t,float l,int op1,int op2,int op3,int op4)
{
  float r1,r2,r3,r4;
  r1 = cal(i,j,op1);
  r2 = cal(k,t,op3);
  r3 = cal(r1,r2,op2);
  r4 = cal(r3,l,op4);
  return r4;
}
float calculate_model4(float i,float j,float k,float t,float l,int op1,int op2,int op3,int op4)
{
  float r1,r2,r3,r4;
  r1 = cal(i,j,op1);
  r2 = cal(k,t,op3);
  r3 = cal(r2,l,op4);
  r4 = cal(r1,r3,op2);
  return r4;
}
float calculate_model5(float i,float j,float k,float t,float l,int op1,int op2,int op3,int op4)
{
  float r1,r2,r3,r4;
  r1 = cal(i,j,op1);
  r2 = cal(t,l,op4);
  r3 = cal(k,r2,op3);
  r4 = cal(r1,r3,op2);
  return r4;
}
float calculate_model6(float i,float j,float k,float t,float l,int op1,int op2,int op3,int op4)
{
  float r1,r2,r3,r4;
  r1 = cal(j,k,op2);
  r2 = cal(i,r1,op1);
  r3 = cal(t,l,op4);
  r4 = cal(r2,r3,op3);
  return r4;
}
float calculate_model7(float i,float j,float k,float t,float l,int op1,int op2,int op3,int op4)
{
  float r1,r2,r3,r4;
  r1 = cal(j,k,op2);
  r2 = cal(i,r1,op1);
  r3 = cal(r2,t,op3);
  r4 = cal(r3,l,op4);
  return r4;
}
float calculate_model8(float i,float j,float k,float t,float l,int op1,int op2,int op3,int op4)
{
  float r1,r2,r3,r4;
  r1 = cal(j,k,op2);
  r2 = cal(r1,t,op3);
  r3 = cal(i,r2,op1);
  r4 = cal(r3,l,op4);
  return r4;
}
float calculate_model9(float i,float j,float k,float t,float l,int op1,int op2,int op3,int op4)
{
  float r1,r2,r3,r4;
  r1 = cal(k,t,op3);
  r2 = cal(j,r1,op2);
  r3 = cal(i,r2,op1);
  r4 = cal(r3,l,op4);
  return r4;
}
float calculate_model10(float i,float j,float k,float t,float l,int op1,int op2,int op3,int op4)
{
  float r1,r2,r3,r4;
  r1 = cal(j,k,op2);
  r2 = cal(r1,t,op3);
  r3 = cal(r2,l,op4);
  r4 = cal(i,r3,op1);
  return r4;
}
float calculate_model11(float i,float j,float k,float t,float l,int op1,int op2,int op3,int op4)
{
  float r1,r2,r3,r4;
  r1 = cal(j,k,op2);
  r2 = cal(t,l,op4);
  r3 = cal(r1,r2,op3);
  r4 = cal(i,r3,op1);
  return r4;
}
float calculate_model12(float i,float j,float k,float t,float l,int op1,int op2,int op3,int op4)
{
  float r1,r2,r3,r4;
  r1 = cal(k,t,op3);
  r2 = cal(j,r1,op2);
  r3 = cal(r2,l,op4);
  r4 = cal(i,r3,op1);
  return r4;
}
float calculate_model13(float i,float j,float k,float t,float l,int op1,int op2,int op3,int op4)
{
  float r1,r2,r3,r4;
  r1 = cal(k,t,op3);
  r2 = cal(r1,l,op4);
  r3 = cal(j,r2,op2);
  r4 = cal(i,r3,op1);
  return r4;
}
float calculate_model14(float i,float j,float k,float t,float l,int op1,int op2,int op3,int op4)
{
  float r1,r2,r3,r4;
  r1 = cal(t,l,op4);
  r2 = cal(k,r1,op3);
  r3 = cal(j,r2,op2);
  r4 = cal(i,r3,op1);
  return r4;
}

int get60(int i,int j,int k,int t,int l)
{
    int op1,op2,op3,op4;
    int flag=0;
    for(op1=1;op1<=4;op1++)
        for(op2=1;op2<=4;op2++)
            for(op3=1;op3<=4;op3++)
		for(op4=1;op4<=4;op4++)
            	{
               	   if(calculate_model1(i,j,k,t,l,op1,op2,op3,op4)==PAI_A){
		       printf("(((%d%c%d)%c%d)%c%d)%c%d\n",i,op[op1],j,op[op2],k,op[op3],t,op[op4],l);
	               flag = 1;goto OUT;
                   }
               	   if(calculate_model2(i,j,k,t,l,op1,op2,op3,op4)==PAI_A){
		       printf("((%d%c%d)%c%d)%c(%d%c%d)\n",i,op[op1],j,op[op2],k,op[op3],t,op[op4],l);
	               flag = 1;goto OUT;
                   }
               	   if(calculate_model3(i,j,k,t,l,op1,op2,op3,op4)==PAI_A){
		       printf("((%d%c%d)%c(%d%c%d))%c%d\n",i,op[op1],j,op[op2],k,op[op3],t,op[op4],l);
	               flag = 1;goto OUT;
                   }
               	   if(calculate_model4(i,j,k,t,l,op1,op2,op3,op4)==PAI_A){
		       printf("(%d%c%d)%c((%d%c%d)%c%d)\n",i,op[op1],j,op[op2],k,op[op3],t,op[op4],l);
	               flag = 1;goto OUT;
                   }
               	   if(calculate_model5(i,j,k,t,l,op1,op2,op3,op4)==PAI_A){
		       printf("(%d%c%d)%c(%d%c(%d%c%d))\n",i,op[op1],j,op[op2],k,op[op3],t,op[op4],l);
	               flag = 1;goto OUT;
                   }
               	   if(calculate_model6(i,j,k,t,l,op1,op2,op3,op4)==PAI_A){
		       printf("(%d%c(%d%c%d))%c(%d%c%d)\n",i,op[op1],j,op[op2],k,op[op3],t,op[op4],l);
	               flag = 1;goto OUT;
                   }
               	   if(calculate_model7(i,j,k,t,l,op1,op2,op3,op4)==PAI_A){
		       printf("((%d%c(%d%c%d))%c%d)%c%d\n",i,op[op1],j,op[op2],k,op[op3],t,op[op4],l);
	               flag = 1;goto OUT;
                   }
               	   if(calculate_model8(i,j,k,t,l,op1,op2,op3,op4)==PAI_A){
		       printf("(%d%c((%d%c%d)%c%d))%c%d\n",i,op[op1],j,op[op2],k,op[op3],t,op[op4],l);
	               flag = 1;goto OUT;
                   }
               	   if(calculate_model9(i,j,k,t,l,op1,op2,op3,op4)==PAI_A){
		       printf("(%d%c(%d%c(%d%c%d)))%c%d\n",i,op[op1],j,op[op2],k,op[op3],t,op[op4],l);
	               flag = 1;goto OUT;
                   }
               	   if(calculate_model10(i,j,k,t,l,op1,op2,op3,op4)==PAI_A){
		       printf("%d%c(((%d%c%d)%c%d)%c%d)\n",i,op[op1],j,op[op2],k,op[op3],t,op[op4],l);
	               flag = 1;goto OUT;
                   }
               	   if(calculate_model11(i,j,k,t,l,op1,op2,op3,op4)==PAI_A){
		       printf("%d%c((%d%c%d)%c(%d%c%d))\n",i,op[op1],j,op[op2],k,op[op3],t,op[op4],l);
	               flag = 1;goto OUT;
                   }
               	   if(calculate_model12(i,j,k,t,l,op1,op2,op3,op4)==PAI_A){
		       printf("%d%c((%d%c(%d%c%d))%c%d)\n",i,op[op1],j,op[op2],k,op[op3],t,op[op4],l);
	               flag = 1;goto OUT;
                   }
               	   if(calculate_model13(i,j,k,t,l,op1,op2,op3,op4)==PAI_A){
		       printf("%d%c(%d%c((%d%c%d)%c%d))\n",i,op[op1],j,op[op2],k,op[op3],t,op[op4],l);
	               flag = 1;goto OUT;
                   }
               	   if(calculate_model14(i,j,k,t,l,op1,op2,op3,op4)==PAI_A){
		       printf("%d%c(%d%c(%d%c(%d%c%d)))\n",i,op[op1],j,op[op2],k,op[op3],t,op[op4],l);
	               flag = 1;goto OUT;
                   }

                }
OUT:    
	return flag;
}


int step_one(int *in)
{
    int x,y,m,n,z;
    int i,j,k,t,l;
    int flag;
    for(i=0;i<5;i++){
        for(j=0;j<5;j++){
            if(j==i) continue;
            for(k=0;k<5;k++){
                if(i==k||j==k) continue;
                for(t=0;t<5;t++){
                    if(t==i||t==j||t==k) continue;
			for(l=0;l<5;l++){
			    if(l==i||l==j||l==k||l==t) continue;
                    	    x = in[i];
                            y = in[j];
                            m = in[k];
                            n = in[t];
			    z = in[l];
                            flag = get60(x,y,m,n,z);
                            if(flag ==1)return 1;
		    }
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
    
    int in[5],i,j,k,l,t,num=0,allnum=0,ret;
    for(i=1;i<PAI_C;i++)
    for(j=i;j<PAI_C;j++)
    for(k=j;k<PAI_C;k++)
    for(l=k;l<PAI_C;l++)
    for(t=l;t<PAI_C;t++)
    {
	in[0]=i;in[1]=j;in[2]=k;in[3]=l;in[4]=t;	
	ret=0;
	ret=step_one(in);
	if(ret==1)num++;
	allnum++;
    }
    printf("%d %d\n",num,allnum);
    return 0;
}
