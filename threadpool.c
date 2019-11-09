//yarin danon
//305413122
#include <stdio.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h>  
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include "threadpool.h"

//create the threadpool
threadpool* create_threadpool(int num_threads_in_pool)
{
	//case of number too big or negative
	if(num_threads_in_pool > MAXT_IN_POOL || num_threads_in_pool < 1)
	{
		return NULL;
	}
	//initialize
	threadpool* tPool = (threadpool*)malloc(sizeof(threadpool));
	pthread_mutex_init(&tPool->qlock,NULL);
	pthread_cond_init(&tPool->q_not_empty,NULL);
	pthread_cond_init(&tPool->q_empty,NULL);
	tPool->num_threads = num_threads_in_pool;
	tPool->qsize = 0;
	tPool->qhead = NULL;
	tPool->qtail = NULL;
	tPool->shutdown = 0;
	tPool->dont_accept = 0;
	tPool->threads = (pthread_t*)malloc(num_threads_in_pool*sizeof(pthread_t));
	int rc, t; 
	//create the threads
	for(t=0;t<num_threads_in_pool;t++)
	{
		rc = pthread_create(&tPool->threads[t], NULL,do_work, tPool); 
		if (rc)
		{ 
			printf("ERROR\n"); 
			return NULL;
		} 
	} 	
	return tPool;	
}
//the method that the thread Entered
void* do_work(void* p)
{
	threadpool *tPool = (threadpool*)p;

	while(1)
	{	
		pthread_mutex_lock (&tPool->qlock);
		//case of shutdown
		if(tPool->shutdown == 1)
		{
			pthread_mutex_unlock(&tPool->qlock);
			return NULL;
		}
		//case of empty list
		if(tPool->qsize == 0)
		{
			pthread_cond_wait(&tPool->q_not_empty, &tPool->qlock); 
		}
		//case of shutdown
		if(tPool->shutdown == 1)
		{
			pthread_mutex_unlock(&tPool->qlock); 
			return NULL;
		} 
		work_t* workNode = tPool->qhead;
		if(workNode == NULL)
		{
			pthread_mutex_unlock(&tPool->qlock);
			continue;
		}
		tPool->qhead = (tPool->qhead)->next;
		tPool->qsize--;
		if((tPool->qsize == 0) && (tPool->dont_accept == 1))
		{
			pthread_mutex_unlock(&tPool->qlock);
			pthread_cond_signal(&tPool->q_empty); 
		}
		pthread_mutex_unlock(&tPool->qlock);
		workNode->routine(workNode->arg);
		free(workNode);
	}

}
//destroy the threadpool
void destroy_threadpool(threadpool* destroyme)
{
	pthread_mutex_lock(&destroyme->qlock);
	destroyme->dont_accept = 1;
	//case that the list not empty
	if(destroyme->qsize > 0)
		pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);
	destroyme->shutdown =1;
	pthread_mutex_unlock(&destroyme->qlock);
	pthread_cond_broadcast(&destroyme->q_not_empty);
	//wait for all the threads 
	for ( int i = 0; i < destroyme->num_threads; i++) 
		pthread_join(destroyme->threads[i], NULL); 
	free(destroyme->threads);
	free(destroyme);
}
//method that Entered a work to the list
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
	pthread_mutex_lock(&from_me->qlock);
	if(from_me->dont_accept == 1)
	{
		pthread_mutex_unlock(&from_me->qlock);
		return;
	}
	//create and initialize the work
	work_t* workNode = (work_t*)malloc(sizeof(work_t));
	workNode->routine = dispatch_to_here;
	workNode->arg = arg;
	workNode->next = NULL;
	//case of empty list
	if(from_me->qsize == 0)
	{
		from_me->qhead = workNode;
		from_me->qtail = workNode; 
	}
	//Entered a work to the list
	else
	{
		from_me->qtail->next = workNode;
		from_me->qtail = workNode;
	}
	from_me->qsize++;
	pthread_cond_signal(&from_me->q_not_empty);
	pthread_mutex_unlock(&from_me->qlock);

}
