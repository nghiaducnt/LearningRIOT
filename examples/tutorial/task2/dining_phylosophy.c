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
#define OTHER_FORK(fork_strategy) ((fork_strategy + 1) % 2)
#define PRINT_FORK(fork) (fork == LEFT ? "Left" : "Right")

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
	int current_strategy;
	int conflick;
	uint32_t num_of_conflick;
};


struct _phylosophy {
	int id;
	int state;
	int current_strategy;
	uint32_t eating;
	int pid;
	char thread_name[32];
	char thread_stack[THREAD_STACKSIZE_DEFAULT];
	struct _fork* fork[2];
	struct _configuration *conf;
}; 

struct _fork {
	int id;
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
	Doctor[index].fork[RIGHT] = &Fork[index];
	memset(Doctor[index].thread_name, 0, 32);
	sprintf(Doctor[index].thread_name, "Phylosopher %d", index);
	return 0;
}
static void phylosophy_print(void)
{
	int i;
	for (i = 0; i < N; i++) {
		printf("Doctor %d ate %ld times\n", i, Doctor[i].eating);
	}
	printf("Configure status: conflicts %ld\n", Conf.num_of_conflick);
}
static int fork_init(int index)
{
	Fork[index].id = index;
	Fork[index].freed = -1;
	Fork[index].owner = NULL;
	Fork[index].times = 0;
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
		case RELEASING:
			printf("Dr %d is releasing\n", dr->id);
			break;
		case THINKING:
			printf("Dr %d is thinking\n", dr->id);
			break;
		default:
			printf("Dr %d is in unknown state %d\n", dr->id, dr->state);
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
		/* If this block of code in the if condition can not be protected by the mutex
		* fork's resource can be used by the other Dr
		*/
		fork->owner = dr;
		if (conf->test_yield)
			thread_yield();
		/* set fork as not free, contains the Dr'id */
		fork->freed = dr->id;
		/* increase number of use */
		fork->times++;
		/* set the Dr owner */
		if (fork->owner == NULL || fork->owner->id != fork->freed) {
			if (fork->owner == NULL)
				printf("Catch you, fork is being used by %d and freed by unknown\n", fork->freed);
			else	
				printf("Catch you, fork is being used by %d and %d\n", fork->freed, fork->owner->id);
			conf->conflick = 1;
			conf->num_of_conflick++;
		}
		if (conf->test_lock == 1)
			mutex_unlock(&mutex);
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
		case RELEASING:
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
	dr->state = HUNGRY;
	phylosophy_rest(dr);
	return 0;
}
static int phylosophy_hungry(struct _phylosophy *dr) {
	/* sanity check */
	if (dr == NULL)
		return -1;
	/* too lazy to take a break before turn to hungry state */
	dr->state = TAKING;
	phylosophy_rest(dr);
	return 0;
}
static int phylosophy_thinking(struct _phylosophy *dr) {
	/* sanity check */
	if (dr == NULL)
		return -1;
	/* too lazy to take a break before turn to hungry state */
	dr->state = HUNGRY;
	phylosophy_rest(dr);
	return 0;
}
static int phylosophy_eating(struct _phylosophy *dr) 
{
	/*  */
	if (dr == NULL)
		return -1;
	/* Dr needs sometimes to eat */
	dr->eating++;
	/* After eating, Dr will release */
	dr->state = RELEASING;
	phylosophy_rest(dr);
	return 0;	
} 

// take up chopsticks 
static int phylosophy_taking(struct _phylosophy *dr) 
{ 
	if (dr == NULL) 
		return -1;
	dr->current_strategy = dr->conf->current_strategy;
	/* next Doctor will have different strategy */
	dr->conf->current_strategy = OTHER_FORK(dr->conf->current_strategy);
	printf("Doctor %d will take fork %s\n", dr->id, PRINT_FORK(dr->current_strategy));
	/* Dr needs sometimes to take forks */
	if (fork_test_and_get(dr->fork[dr->current_strategy], dr, dr->conf) == 1) {
		printf("Doctor %d took fork %d on the %s\n", dr->id, dr->fork[dr->current_strategy]->id,
			PRINT_FORK(dr->current_strategy));
		if (fork_test_and_get(dr->fork[OTHER_FORK(dr->current_strategy)], dr, dr->conf) == 1) {
			printf("Doctor %d took fork %d on the %s\n", dr->id, dr->fork[OTHER_FORK(dr->current_strategy)]->id,
				PRINT_FORK(OTHER_FORK(dr->current_strategy)));
			/* After taking successfully, Dr will eat */
			dr->state = EATING;
		} else {
			/* Doctor cannot get RIGHT fork, don't forget release LEFT for other */
			printf("Doctor %d could not take fork %d on the %s\n", dr->id, dr->fork[OTHER_FORK(dr->current_strategy)]->id, 
				PRINT_FORK(OTHER_FORK(dr->current_strategy)));
			fork_release(dr->fork[dr->current_strategy], dr->conf);
		}
	} else {
		printf("Doctor %d could not take the other fork %d on the %s\n", dr->id, dr->fork[dr->current_strategy]->id,
			PRINT_FORK(dr->current_strategy));
	}
	phylosophy_rest(dr);
	return 0;
} 

// put down chopsticks 
static int phylosophy_releasing(struct _phylosophy *dr) 
{ 
	if (dr == NULL)
		return -1;
	if (dr->fork[LEFT]->owner != dr ||
		dr->fork[RIGHT]->owner != dr)
		printf("Catch you: Doctor %d release fork %d and fork %d which have not been taken by him\n", dr->id,
			dr->fork[LEFT]->id,
			dr->fork[RIGHT]->id);
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
				printf("Unknow state %d %d\n", dr->id, dr->state);
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
		Conf.test_lock = 0; /* create resource conflict */
		Conf.current_strategy = LEFT;
		Conf.conflick = 0;
		Conf.num_of_conflick = 0;
		for (i = 0; i < N; i++) { 
			fork_init(i);
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
		for (i = 0; i < N; ) {
			if (Doctor[i].eating == 0) {
				thread_yield();
			} else {
				i++;
			}
			continue;
		}
		bStop = 1;
		phylosophy_print(); 
	} else
		bStop = 1;
	return 0;
} 

