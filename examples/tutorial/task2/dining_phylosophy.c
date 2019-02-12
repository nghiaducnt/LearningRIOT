#include <stdio.h> 
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "mutex.h"
#include "thread.h"
#include "algorithm.h"

#define N 5 

#define LEFT 0
#define RIGHT 1

#define LEFT_FORK(id) ((id + (N-1)) % N)
#define RIGHT_FORK(id) ((id + 1) %N)
int phil[N] = { 0, 1, 2, 3, 4 }; 
mutex_t mutex;
static int bStop = 1;
enum State {
	LAZY = 0,
	HUNGRY,
	TAKING,
	EATING,
	RELEASING,
	THINKING,
};
struct _configuration {
	int test_lock;
	int test_yield;
	int conflick;
	uint32_t num_of_conflick;
};


struct _phylosophy {
	int id;
	int state;
	uint32_t eating;
	int pid;
	char thread_name[32];
	char thread_stack[THREAD_STACKSIZE_DEFAULT];
	struct _fork* fork[2];
	struct _configuration *conf;
}; 

struct _fork {
	int freed;
	struct _phylosophy *owner;
	uint32_t times; 
};
static struct _phylosophy Doctor[N];
static struct _fork Fork[N];
static struct _configuration Conf;

static int phylosophy_init(int index, struct _configuration *conf)
{
	if (index < 0 || index >= N)
		return -1;
	Doctor[index].conf = conf;
	Doctor[index].eating = 0;
	Doctor[index].id = index;
	Doctor[index].fork[LEFT] = &Fork[LEFT_FORK(index)];
	Doctor[index].fork[RIGHT] = &Fork[RIGHT_FORK(index)];
	memset(Doctor[index].thread_name, 0, 32);
	sprintf(Doctor[index].thread_name, "Phylosopher %d", index);
	return 0;
}
static int phylosophy_printstate(struct _phylosophy *dr)
{
	if (dr == NULL)
		return -1;
	switch (dr->state) {
		case LAZY:
			printf("Dr %d is lazy\n", dr->id);
			break;
		case HUNGRY:
			printf("Dr %d is hungry\n", dr->id);
			break;
		case TAKING:
			printf("Dr %d is taking\n", dr->id);
			break;
		case EATING:
			printf("Dr %d is eating\n", dr->id);
			break;
		case THINKING:
			printf("Dr %d is think\n", dr->id);
			break;
		default:
			printf("Dr %d is in unknown state\n", dr->id);
			break;
	}
	return 0;
}
static int fork_test_and_get(struct _fork *fork, struct _phylosophy *dr, struct _configuration *conf)
{
	if (fork == NULL || dr == NULL || conf == NULL)
		return -1;
	/* lock */
	if (conf->test_lock == 1)
		mutex_lock(&mutex);
	if (fork->freed == -1) {
		/* set fork as not free, contains the Dr'id */
		fork->freed = dr->id;
		if (conf->test_yield)
			thread_yield();
		/* increase number of use */
		fork->times++;
		/* set the Dr owner */
		fork->owner = dr;
		if (fork->owner->id != fork->freed) {
			printf("Catch you, fork is being used by %d and %d\n", fork->freed, fork->owner->id);
			conf->conflick = 1;
			conf->num_of_conflick++;
		}
		return 1;
	}
	/* unlock */
	if (conf->test_lock == 1)
		mutex_unlock(&mutex);
	return 0;
}
static int fork_release(struct _fork *fork, struct _configuration *conf)
{
	if (fork == NULL || conf == NULL)
		return -1;
	/* lock */
	if (conf->test_lock == 1)
		mutex_lock(&mutex);
	fork->freed = -1;
	/* set the Dr owner */
	fork->owner = NULL;
	/* unlock */
	if (conf->test_lock == 1)
		mutex_unlock(&mutex);
	return 0;
}
static int phylosophy_rest(struct _phylosophy *dr)
{
	if (dr == NULL)
		return -1;
	phylosophy_printstate(dr);
	switch (dr->state) {
		case LAZY:
		case HUNGRY:
		case TAKING:
		case EATING:
		case THINKING:
			thread_yield();
			break;
		default:
			return -1;
	}
	return 0;
}
static int phylosophy_lazy(struct _phylosophy *dr) {
	/* sanity check */
	if (dr == NULL)
		return -1;
	/* too lazy to take a break before turn to hungry state */
	if (phylosophy_rest(dr) == 0)
		dr->state = HUNGRY;
	return 0;
}
static int phylosophy_hungry(struct _phylosophy *dr) {
	/* sanity check */
	if (dr == NULL)
		return -1;
	/* too lazy to take a break before turn to hungry state */
	if (phylosophy_rest(dr) == 0)
		dr->state = TAKING;
	return 0;
}
static int phylosophy_thinking(struct _phylosophy *dr) {
	/* sanity check */
	if (dr == NULL)
		return -1;
	/* too lazy to take a break before turn to hungry state */
	if( phylosophy_rest(dr) == 0)
		dr->state = HUNGRY;
	return 0;
}
static int phylosophy_eating(struct _phylosophy *dr) 
{
	/*  */
	if (dr == NULL)
		return -1;
	/* Dr needs sometimes to eat */
	/* After eating, Dr will think */
	if (phylosophy_rest(dr) == 0) {
		dr->eating++;
		/* After eating, Dr will think */
		dr->state = RELEASING;
	}
	return 0;	
} 

// take up chopsticks 
static int phylosophy_taking(struct _phylosophy *dr) 
{ 
	/*  */
	if (dr == NULL) 
		return -1;
	/* Dr needs sometimes to take forks */
	if (fork_test_and_get(dr->fork[LEFT], dr, dr->conf) == 1) {
		if (fork_test_and_get(dr->fork[RIGHT], dr, dr->conf) == 1) {
			/* After taking successfully, Dr will eat */
			if( phylosophy_rest(dr) == 0)
				dr->state = EATING;
		} else {
			/* Doctor cannot get RIGHT fork, don't forget release LEFT for other */
			fork_release(dr->fork[LEFT], dr->conf);
		}
	}
	return 0;
} 

// put down chopsticks 
static int phylosophy_releasing(struct _phylosophy *dr) 
{ 
	if (dr == NULL)
		return -1;
	/* Dr needs sometimes to release */
	fork_release(dr->fork[LEFT], dr->conf);
	fork_release(dr->fork[RIGHT], dr->conf);
	if( phylosophy_rest(dr) == 0)
		dr->state = THINKING;
	return 0;	
} 

static void* philosopher(void* arg) 
{
	struct _phylosophy *dr;
	if (arg == NULL)
		return NULL;
	dr = (struct _phylosophy *)arg;
	dr->eating = 0;
	while (bStop == 0) {
		switch (dr->state) {
			case LAZY:
				phylosophy_lazy(dr);
				break;
			case HUNGRY:
				phylosophy_hungry(dr);
				break;
			case TAKING:
				phylosophy_taking(dr);
				break;
			case EATING:
				phylosophy_eating(dr);
				break;
			case RELEASING:
				phylosophy_releasing(dr);
				break;
			case THINKING:
				phylosophy_thinking(dr);
				break;
			default:
				printf("Unknow state %d\n", dr->id);
				break;
		}
	}
	return NULL; 
} 
static int bInit = 0;
int dining_phylosophy(void) 
{ 

	int i; 

	// initialize the semaphorea
	if (bInit == 0) { 
		bInit = 1;
		mutex_init(&mutex);
	}
	if (bStop == 1) {
		bStop = 0;
		Conf.test_yield = 1;
		Conf.test_lock = 1;
		Conf.conflick = 0;
		Conf.num_of_conflick = 0;
		for (i = 0; i < N; i++) { 
			phylosophy_init(i, &Conf);
			/* start thread */
			if ((Doctor[i].pid = thread_create(Doctor[i].thread_stack, sizeof(Doctor[i].thread_stack), THREAD_PRIORITY_MAIN ,
				THREAD_CREATE_STACKTEST,
				philosopher, &Doctor[i], Doctor[i].thread_name)) <= KERNEL_PID_UNDEF) {
				printf("error initializing thread\n");
				bStop = 1;
				return 1;
			}
    		} 
		for (i = 0; ; i++) {
			if (i == N)
				i = 0;
			if (Doctor[i].eating == 0) {
				thread_yield();
				continue;
			}
		bStop = 1;
		break;
		} 
	} else
	bStop = 1;
	return 0;
} 

