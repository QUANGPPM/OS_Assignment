
#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
//add thêm biến hỗ trợ
static int current_prio;
static int remain_slot[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i ;
	for (i = 0; i < MAX_PRIO; i ++){
		mlq_ready_queue[i].size = 0;
		//init các biến mới thêm:
		remain_slot[i] = MAX_PRIO - i;
	}
#endif
	ready_queue.size = 0;		// khi không kích hoạt MLQ_SCHED
	run_queue.size = 0;
	current_prio = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED	
//==============================================================//
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
	struct pcb_t * get_mlq_proc(void) {
		struct pcb_t * proc = NULL;
		static int check = 0; // dùng để check thử đã kiểu tra đủ MAX_PRIO queue chưa
		static int enable = 0;
		/*TODO: get a process from PRIORITY [ready_queue].
		* Remember to use lock to protect the queue.
		* */
		
		while (proc == NULL) {
        // Kiểm tra slot còn lại trước khi khóa mutex
        if (remain_slot[current_prio] > 0) {	
            pthread_mutex_lock(&queue_lock);

            // Kiểm tra nếu hàng đợi không rỗng
            if (!empty(&mlq_ready_queue[current_prio])) {
                proc = dequeue(&mlq_ready_queue[current_prio]);
                remain_slot[current_prio]--; // Giảm slot của hàng đợi hiện tại
#ifdef REMAIN_SLOT	
				printf("RQ[%d] remain: %d slot\n",current_prio ,remain_slot[current_prio]);
#endif			
				enable = 0;
				check = 0;						// gán check = 0
				current_prio = 0;					// reset lại để kiểm tra mlq_queue lại từ đầu
                pthread_mutex_unlock(&queue_lock); // Mở khóa mutex trước khi thoát vòng lặp
                break;
            }
            pthread_mutex_unlock(&queue_lock); // Mở khóa nếu hàng đợi rỗng
        }
		check++;
		// nếu kiểm tra đủ 140 queue trống -> thoát 
		if(enable && check >= MAX_PRIO && proc == NULL){
			current_prio = 0;
			// printf("check = %d\n", check);
			check = 0;
			enable = 0;
			return NULL;
		}
		// printf("RQ[%d]\n",current_prio);
        // Chuyển sang hàng đợi tiếp theo
        current_prio = (current_prio + 1) % MAX_PRIO;
        // Reset slot nếu đã duyệt qua toàn bộ hàng đợi
        if (current_prio == 0 && proc == NULL && check == MAX_PRIO) {
// #ifdef REMAIN_SLOT	
// 				printf("reset slot!\n");
// #endif	
            for (int i = 0; i < MAX_PRIO; i++) {
                remain_slot[i] = MAX_PRIO - i;
            }
			check = 0;
			enable = 1;
        }
    }

		return proc;	
	}
//==============================================================//
/*
 * Đưa tiến trình chưa hoàn thành trở lại hàng đợi để thực thi sau.
 * Tiến trình được đưa vào đúng hàng đợi dựa trên mức ưu tiên.
 */
void put_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}
//==============================================================//
//thêm tiến trình vào hàng đợi tương ứng với mức ưu tiên
void add_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);	
}
//==============================================================//
struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
	return put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
	return add_mlq_proc(proc);
}
#else
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	return proc;
}

void put_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif


