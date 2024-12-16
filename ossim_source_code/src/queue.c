#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) { //check hàm add_mlq_proc bên sched.c để hiểu cách sử dụng
        /* TODO: put a new process to queue [q] */
        if (q->size >= MAX_QUEUE_SIZE) {
                printf("Queue for prio %d is full. Can't enqueue process %d.\n",
                proc->prio, proc->pid);
                return;
        }
        // them tien trinh vao cuoi hang doi
        q->proc[q->size] = proc;
        //tang kich thuoc
        q->size++;

}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * lấy pcb_t, xóa và sắp xếp lại mảng 
         * */
        if (empty(q)){
                return NULL;
        }
        //get pcb
        struct pcb_t * temp = q->proc[0];
        
        //arrange again:
        for(int i = 0; i < q->size - 1; i++){
                q->proc[i] = q->proc[i+1];
        }
        //decrease the size
        q->size--;

        return temp;
        
}

