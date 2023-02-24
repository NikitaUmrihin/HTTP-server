#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "threadpool.h"

//  more notes and explanations are in 'threadpool.h'

threadpool* create_threadpool(int num_threads_in_pool)
{
    // input sanity check 
    if(num_threads_in_pool<1 || num_threads_in_pool>MAXT_IN_POOL)
    {
        printf("Usage: threadpool <pool-size> <max-number-of-jobs>\n");
        exit(1);
    }
    
    threadpool* pool = (threadpool*) malloc(sizeof(threadpool));
    assert(pool!=NULL);
    
    pool->num_threads = num_threads_in_pool;
    pool->qsize = 0;
    pool->qhead = NULL;
    pool->qtail = NULL;
    pool->shutdown = 0;
    pool->dont_accept = 0;
    
    //  threads = pointer to <num_threads> threads;
    pthread_t * threads = (pthread_t *) malloc((sizeof(pthread_t)) * num_threads_in_pool);
    assert(threads!=NULL);
    pool->threads = threads;
    
    
    //  initialize mutex
    if( pthread_mutex_init(&(pool->qlock), NULL)!=0 )
    {
        perror("mutex initialization failure\n");
        exit(1);
    }

    //  initialize condition variables
    if( pthread_cond_init(&(pool->q_not_empty), NULL)!=0 )
    {
        perror("condition initialization failure\n");
        exit(1);
    }
    
    if( pthread_cond_init(&(pool->q_empty), NULL)!=0 )
    {
        perror("condition initialization failure\n");
        exit(1);
    }
    
    //  create threads with 'do_work' as function and pool as argument
    int status;
    for (int i = 0; i < num_threads_in_pool; i++) 
    {
        status = pthread_create( &pool->threads[i], NULL, do_work, (void *) pool );
        if (status != 0) 
        {
            perror("pthread_create\n");
            exit(1);
        }
    }
    
    return pool;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//..............................................................................................
////////////////////////////////////////////////////////////////////////////////////////////////

void* do_work(void* p)
{
    threadpool* pool = (threadpool*) p;
    
    while(1)
    {
        pthread_mutex_lock(&(pool->qlock));
        //  if destruction begun , exit thread
        if( pool->shutdown == 1 )
        {
            pthread_mutex_unlock(&(pool->qlock));
            pthread_exit(NULL);
        }    
        //  if q is empty , wait
        if( pool->qsize == 0 )
        {
            if (pthread_cond_wait(&(pool->q_not_empty), &(pool->qlock)) != 0) 
            {                                  
                perror("pthread_cond_timedwait() error");                                   
                exit(7);
            }
        }
        
        //  check destruction flag again
        if( pool->shutdown == 1 )
        {
            pthread_mutex_unlock(&(pool->qlock));
            return NULL;
        }
        
        //  take first job from queue
        work_t* job = NULL;
        if(pool->qsize>0)
        {
            job = pool->qhead;
            pool->qsize--;
            pool->qhead = pool->qhead->next;
            
            if(pool->qhead==NULL)
                pool->qtail=NULL;
            
            if(job->routine==NULL)
            {
                free(job);
                return NULL;
            }
        }
        
        //  if q is empty, signal destruction
        if(pool->qsize == 0 && pool->dont_accept == 1)
            pthread_cond_signal(&(pool->q_empty));
        
        pthread_mutex_unlock(&(pool->qlock));
        
        //  call thread routine
        if (job!=NULL)
        {
            job->routine(job->arg);
            free(job);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////
//..............................................................................................
////////////////////////////////////////////////////////////////////////////////////////////////

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
    
    pthread_mutex_lock(&(from_me->qlock));
    
    if( from_me->dont_accept == 1 )
    {
        pthread_mutex_unlock(&(from_me->qlock));
        return;
    }
    
    pthread_mutex_unlock(&from_me->qlock);

    //  create and initialize job
    work_t* job = (work_t*) malloc(sizeof(work_t));
    assert(job!=NULL);
        
    job->routine = dispatch_to_here;
    job->arg = arg;
    job->next = NULL;
    
    pthread_mutex_lock(&from_me->qlock);
    
    //  add job to q
    if(from_me->qsize==0)
        from_me->qhead = job;
    else
        from_me->qtail->next = job;
        
    from_me->qtail = job;
    from_me->qsize++;
        
    pthread_mutex_unlock(&(from_me->qlock));
    pthread_cond_signal(&(from_me->q_not_empty));
    
}

////////////////////////////////////////////////////////////////////////////////////////////////
//..............................................................................................
////////////////////////////////////////////////////////////////////////////////////////////////

void destroy_threadpool(threadpool* destroyme)
{
    
    pthread_mutex_lock(&(destroyme->qlock)); 
    
    destroyme->dont_accept = 1 ;
    
    //  wait for the queue to become empty
    while( destroyme->qsize != 0 )
    {
        if (pthread_cond_wait(&(destroyme->q_empty), &(destroyme->qlock)) != 0) 
        {                                  
            perror("pthread_cond_timedwait() error");                                   
            exit(7);
        }
        
    }
    //  check destruction flag
    destroyme->shutdown = 1 ;
    
    //  signal threads that wait on ‘empty queue’, so they can wake up, see the shutdown flag, and exit.
    pthread_cond_broadcast(&(destroyme->q_not_empty));
    
    pthread_mutex_unlock(&(destroyme->qlock));  
    
    //  join all threads
    for(int i = 0; i < destroyme->num_threads ; i++)
        pthread_join(destroyme->threads[i],NULL);

    //  free memory
    pthread_mutex_destroy(&(destroyme->qlock));
    pthread_cond_destroy(&(destroyme->q_not_empty));
    pthread_cond_destroy(&(destroyme->q_empty));
    free(destroyme->threads);
    free(destroyme);
}
