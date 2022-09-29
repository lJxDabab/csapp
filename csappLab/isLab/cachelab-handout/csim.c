#include "cachelab.h"
#include <stdlib.h>
#include <unistd.h>
#include<stdio.h>
#include <limits.h>
#include <getopt.h>
#include <stddef.h>
#include <string.h>
 typedef struct cache
 {
   int valid_bit;
   int tag;
   int timeStamp;
 }cache;
 typedef struct count
 {
   int count_hit;
   int count_miss;
   int count_eviction;
 }count;
 void update(unsigned int address,int s,int E,int b,cache** cache_col,count*count_a);
 void LoadinData(int s,int local_s,int local_t,int E,cache** cache_col,count*count_a);
int main(int argc,char**argv)
{
  FILE *fd;
    int num=0;
     int s,E;
    int b=1;
     char filename[100];
    char operation;
    unsigned int address;
    int size;
    count *count_a;
    while((num=getopt(argc,argv,"hvs:E:b:t:"))!=-1)
    {
      switch (num)
      {
      case 's':
       s=atoi(optarg);
        break;
      case 'E':
      E=atoi(optarg);
      break;
      case 'b':
      b=atoi(optarg);
      break;
      case 't':
      strcpy(filename,optarg);
        break;
        default:
        break;
      }
    }
    count_a=(count*)malloc(sizeof(count));
     count_a->count_eviction=0;
     count_a->count_hit=0;
     count_a->count_miss=0;
    //below are the cache init
    cache** cache_col=(cache**)malloc((1<<s)*sizeof(cache*));
    for(int i=0;i<(1<<s);i++)
    {
      *(cache_col+i)=(cache*)malloc(E*sizeof(cache));
      for(int j=0;j<E;j++)
      {
        cache_col[i][j].valid_bit=0;
        cache_col[i][j].timeStamp=0;
        cache_col[i][j].tag=0;
      }
    }
    fd=fopen(filename,"r");
    while(fscanf(fd," %c %x,%d\n",&operation,&address,&size)!=EOF)
    {
        switch (operation)
        {
        case 'I': 
          break;
        case 'M':
        update(address,s,E,b,cache_col,count_a);
        update(address,s,E,b,cache_col,count_a);
        break;
        case 'L':
        update(address,s,E,b,cache_col,count_a);
        break;
        case 'S':
        update(address,s,E,b,cache_col,count_a);
        break;
        default:
        printf("no such command over MyLittleCache");
          break;
        }
    }
    fclose(fd);
    printSummary(count_a->count_hit,count_a->count_miss, count_a->count_eviction);
    
    return 0;
}
 void update(unsigned int address,int s,int E,int b,cache** cache_col,count*count_a)
 {
     unsigned local_s,local_t;
     int turn=0,i;
     local_s=((address&(0xffffffff>>(64-s)<<b))>>b);
     local_t=address>>(s+b);
   for(i=0;i<(1<<s);i++)
   {
     for(int j=0;j<E;j++)
     {
       if(cache_col[i][j].timeStamp)
       {
         cache_col[i][j].timeStamp++;
       }
     }
   }
     for(i=0;i<E;i++)
     {
       if(cache_col[local_s][i].valid_bit&&cache_col[local_s][i].tag==local_t)
       {
             turn=1;
             cache_col[local_s][i].timeStamp=1;
       }
     }
     if(turn==1)
     {
       count_a->count_hit++;
     }
     else{
       count_a->count_miss++;
       LoadinData(s,local_s,local_t,E,cache_col,count_a);
     }
 }
 void LoadinData(int s,int local_s,int local_t,int E,cache** cache_col,count*count_a){
   int max;
   int row;
   int turn=0;
   for(int i=0;i<E;i++)
   {
    if(cache_col[local_s][i].valid_bit==0)
    {
            cache_col[local_s][i].valid_bit=1;
            cache_col[local_s][i].tag=local_t;
            cache_col[local_s][i].timeStamp=1;
            turn=1;
            break;
    }
   }
     if(!turn)
    {
      max=cache_col[local_s][0].timeStamp;
      row=0;
      for(int j=0;j<E;j++)
      {
        if(cache_col[local_s][j].timeStamp>max)
        {
          max=cache_col[local_s][j].timeStamp;
          row=j;
        }
      }   
    cache_col[local_s][row].tag=local_t;
    count_a->count_eviction++;
    cache_col[local_s][row].timeStamp=1;
    }
 }
 