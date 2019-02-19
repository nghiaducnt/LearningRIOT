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

#define PRINT_MSG(fmt, ...) \
            do { if (_print) printf(fmt, __VA_ARGS__); } while (0)
static int bStop = 1;
static int bInit = 0;
static bool _print = 0;
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
	mutex_t mutex;
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
	if (bInit == 0)
		mutex_init(&Fork[index].mutex);
	return 0;
}
static int phylosophy_printstate(struct _phylosophy *dr)
{
	if (dr == NULL)
		return -1;
	switch (dr->state) {
		case LAZY:
			PRINT_MSG("Dr %d is lazy\n", dr->id);
			break;
		case HUNGRY:
			PRINT_MSG("Dr %d is hungry\n", dr->id);
			break;
		case TAKING:
			PRINT_MSG("Dr %d is taking\n", dr->id);
			break;
		case EATING:
			PRINT_MSG("Dr %d is eating\n", dr->id);
			break;
		case RELEASING:
			PRINT_MSG("Dr %d is releasing\n", dr->id);
			break;
		case THINKING:
			PRINT_MSG("Dr %d is thinking\n", dr->id);
			break;
		default:
			PRINT_MSG("Dr %d is in unknown state %d\n", dr->id, dr->state);
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
		mutex_lock(&fork->mutex);
	if (fork->freed == -1) {
		/* If this block of code in the if condition can not be protected by the mutex
		* fork's resource can be used by the other Dr
		*/
		/* set the Dr owner */
		fork->owner = dr;
		if (conf->test_yield)
			thread_yield();
		/* set fork as not free, contains the Dr'id */
		fork->freed = dr->id;
		if (fork->owner == NULL || fork->owner->id != fork->freed) {
			if (fork->owner == NULL)
				PRINT_MSG("Catch you, fork is being used by %d and freed by unknown\n", fork->freed);
			else	
				PRINT_MSG("Catch you, fork is being used by %d and %d\n", fork->freed, fork->owner->id);
			conf->conflick = 1;
			conf->num_of_conflick++;
			if (conf->test_lock == 1)
				mutex_unlock(&fork->mutex);
			return 0;
		}
		/* increase number of use */
		fork->times++;
		if (conf->test_lock == 1)
			mutex_unlock(&fork->mutex);
		return 1;
	}
	/* unlock */
	if (conf->test_lock == 1)
		mutex_unlock(&fork->mutex);
	return 0;
}
static int fork_release(struct _fork *fork, struct _configuration *conf)
{
	if (fork == NULL || conf == NULL)
		return -1;
	/* lock */
	if (conf->test_lock == 1)
		mutex_lock(&fork->mutex);
	fork->freed = -1;
	/* set the Dr owner */
	fork->owner = NULL;
	/* unlock */
	if (conf->test_lock == 1)
		mutex_unlock(&fork->mutex);
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

static int phylosophy_taking(struct _phylosophy *dr) 
{ 
	if (dr == NULL) 
		return -1;
	dr->current_strategy = dr->conf->current_strategy;
	/* next Doctor will have different strategy */
	dr->conf->current_strategy = OTHER_FORK(dr->conf->current_strategy);
	PRINT_MSG("Doctor %d will take fork %s\n", dr->id, PRINT_FORK(dr->current_strategy));
	/* Dr needs sometimes to take forks */
	if (fork_test_and_get(dr->fork[dr->current_strategy], dr, dr->conf) == 1) {
		PRINT_MSG("Doctor %d took fork %d on the %s\n", dr->id, dr->fork[dr->current_strategy]->id,
			PRINT_FORK(dr->current_strategy));
		if (fork_test_and_get(dr->fork[OTHER_FORK(dr->current_strategy)], dr, dr->conf) == 1) {
			PRINT_MSG("Doctor %d took fork %d on the %s\n", dr->id, dr->fork[OTHER_FORK(dr->current_strategy)]->id,
				PRINT_FORK(OTHER_FORK(dr->current_strategy)));
			/* After taking successfully, Dr will eat */
			dr->state = EATING;
		} else {
			/* Doctor cannot get RIGHT fork, don't forget release LEFT for other */
			PRINT_MSG("Doctor %d could not take fork %d on the %s\n", dr->id, dr->fork[OTHER_FORK(dr->current_strategy)]->id, 
				PRINT_FORK(OTHER_FORK(dr->current_strategy)));
			fork_release(dr->fork[dr->current_strategy], dr->conf);
		}
	} else {
		PRINT_MSG("Doctor %d could not take the other fork %d on the %s\n", dr->id, dr->fork[dr->current_strategy]->id,
			PRINT_FORK(dr->current_strategy));
	}
	phylosophy_rest(dr);
	return 0;
} 

static int phylosophy_releasing(struct _phylosophy *dr) 
{ 
	if (dr == NULL)
		return -1;
	if (dr->fork[LEFT]->owner != dr ||
		dr->fork[RIGHT]->owner != dr)
		PRINT_MSG("Catch you: Doctor %d release fork %d and fork %d which have not been taken by him\n", dr->id,
			dr->fork[LEFT]->id,
			dr->fork[RIGHT]->id);
	/* Dr needs sometimes to release */
	fork_release(dr->fork[LEFT], dr->conf);
	fork_release(dr->fork[RIGHT], dr->conf);
	dr->state = THINKING;
	phylosophy_rest(dr);
	return 0;	
} 

/* inside a Phylosoher's mind */
static void* phylosopher_mind(void* arg) 
{
	struct _phylosophy *dr;
	if (arg == NULL)
		return NULL;
	dr = (struct _phylosophy *)arg;
	dr->eating = 0;
	while (bStop == 0 || dr->state != HUNGRY) {
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
				PRINT_MSG("Unknow state %d %d\n", dr->id, dr->state);
				break;
		}
	}
	return NULL; 
} 
int dining_phylosophy(int times, int yield, int lock, int silent) 
{ 

	int i;
	(void)silent;
	if (bStop == 1) {
		bStop = 0;
		Conf.test_yield = yield;
		Conf.test_lock = lock; /* create resource conflict */
		_print = silent;
		Conf.current_strategy = LEFT;
		Conf.conflick = 0;
		Conf.num_of_conflick = 0;
		for (i = 0; i < N; i++) { 
			fork_init(i);
			phylosophy_init(i, &Conf);
			/* start thread */
			if ((Doctor[i].pid = thread_create(Doctor[i].thread_stack, sizeof(Doctor[i].thread_stack), THREAD_PRIORITY_MAIN ,
				THREAD_CREATE_STACKTEST,
				phylosopher_mind, &Doctor[i], Doctor[i].thread_name)) <= KERNEL_PID_UNDEF) {
				printf("error initializing thread\n");
				bStop = 1;
				return 1;
			}
    		}
		bInit = 1; 
		for (i = 0; i < N; ) {
			if (Doctor[i].eating < (uint32_t)times) {
				thread_yield();
			} else {
				i++;
			}
			continue;
		}
		bStop = 1;
		for (i = 0; i < N; ) {
			if (Doctor[i].state != HUNGRY) {
				thread_yield();
			} else {
				i++;
			}
			continue;
		}
		phylosophy_print(); 
	} else
		bStop = 1;
	return 0;
} 

