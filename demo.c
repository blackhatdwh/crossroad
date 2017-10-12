/*
 * demo.c
 * Copyright (C) 2017 weihao <weihao@weihao-PC>
 *
 * Distributed under terms of the MIT license.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_CAR_NUM 10000      // define the max amount of cars on the road

// 0: NORTH  1: SOUTH  2: WEST  3: EAST
char* direction[4] = {"NORTH", "SOUTH", "WEST", "EAST"};

// a linked list is used to store pthread_t of those currently alive threads
// one thread represent one car. one thread in this linked list represent a car on the road
typedef struct car_on_road* car_ptr;
struct car_on_road {
    pthread_t thread_id;        // the thread id of the car
    int direction;      // the direction of the car
    int num;        // the serial number of the car
    car_ptr next;       // point to the next node
    car_ptr prev;       // point to the previous node
};
car_ptr head;       // actually it is an array of struct car_on_road, which has 4 elements, one direction one linked list
car_ptr tail;

// condition variables used to set coming cars to wait in queue when there are cars in front of it
pthread_cond_t wait_in_queue_cond[4];
pthread_mutex_t wait_in_queue_mutex[4];

/*
    N0
  ---------
  | 0 | 1 |E3
  ---------
W2| 2 | 3 |
  ---------
        S1
*/
// 4 mutexs represents the 4 quadrants
pthread_mutex_t quadrant_mutex[4];

// used to keep a prepared car waiting outside the crossing
pthread_cond_t wait_in_crossing_cond[4];
pthread_mutex_t wait_in_crossing_mutex[4];

pthread_mutex_t deadlock_detect_mutex = PTHREAD_MUTEX_INITIALIZER;      // ensure that no more than one detect routine can be running


int total_car_num = 0;      // indicates how many cars have been created since the program started
int current_car_num = 0;        // indicates how many cars are on the road currently
int car_in_queue[4] = {0, 0, 0, 0};       // indicates the number of cars in queue in each direction

// detect whether there is a deadlock or not
void* DeadLockDetect(void* arg){
    // no more than one of this routine can be running at the same time
    pthread_mutex_lock(&deadlock_detect_mutex);
    int deadlock_flag = 1;
    for(int i = 0; i < 4; i++){     // successivelly visit the four quadrant_mutexes
        int return_code = pthread_mutex_trylock(&quadrant_mutex[i]);
        // if one of the four mutexs can be locked, then there is no deadlock
        if(return_code == 0){
            pthread_mutex_unlock(&quadrant_mutex[i]);
            deadlock_flag = 0;
            break;
        }
    }
    // if 4 quadrant mutex are all acquired, then a deadlock appears
    if(deadlock_flag){
        printf("DEADLOCK: car jam detected, signalling North to go.\n");        // alert user
        usleep(8000);       // magic

        // signal the north car to go first
        pthread_mutex_lock(&wait_in_crossing_mutex[0]);
        pthread_cond_signal(&wait_in_crossing_cond[0]);
        pthread_mutex_unlock(&wait_in_crossing_mutex[0]);
    }
    // unlock so other detection routine can work
    pthread_mutex_unlock(&deadlock_detect_mutex);
}

void Passing(car_ptr car, int first_target, int second_target, int left, int subsequent){
    pthread_mutex_lock(&quadrant_mutex[first_target]);
    printf("car %d from %s prepared.\n", car->num, direction[car->direction]);
    // start the DeadLockDetection routine
    pthread_t deadlock_thread_id;
    pthread_create(&deadlock_thread_id, NULL, DeadLockDetect, NULL);

    usleep(5000);       // observe around before go through the crossing
    int return_code = pthread_mutex_trylock(&quadrant_mutex[second_target]);
    // successfully locked the second mutex, so it can go directly
    if(return_code == 0){
        printf("car %d from %s leaves.\n", car->num, direction[car->direction]);
        // unlock its own two quadrants
        pthread_mutex_unlock(&quadrant_mutex[first_target]);
        pthread_mutex_unlock(&quadrant_mutex[second_target]);
        // notify its left car to go
        pthread_mutex_lock(&wait_in_crossing_mutex[left]);
        pthread_cond_signal(&wait_in_crossing_cond[left]);
        pthread_mutex_unlock(&wait_in_crossing_mutex[left]);
        usleep(5000);       // sleep 5ms to let left car go first
        // notify its subsequent cars to move forward once
        pthread_cond_broadcast(&wait_in_queue_cond[subsequent]);
        pthread_mutex_unlock(&wait_in_queue_mutex[subsequent]);
        pthread_exit(NULL);
    }
    // else, wait for signal. signal can be produced by DeadLockDetection or right car
    else{
        pthread_mutex_lock(&wait_in_crossing_mutex[car->direction]);
        pthread_cond_wait(&wait_in_crossing_cond[car->direction], &wait_in_crossing_mutex[car->direction]);
        pthread_mutex_unlock(&wait_in_crossing_mutex[car->direction]);
    }
    printf("car %d from %s leaves.\n", car->num, direction[car->direction]);     // when signaled, go directly
    pthread_mutex_unlock(&quadrant_mutex[first_target]);       // unlock its own one quadrants
    // notify its left car to go
    pthread_mutex_lock(&wait_in_crossing_mutex[left]);
    pthread_cond_signal(&wait_in_crossing_cond[left]);
    pthread_mutex_unlock(&wait_in_crossing_mutex[left]);
    usleep(5000);       // sleep 5ms to let left car go first
    // notify its subsequent cars to move forward once
    pthread_cond_broadcast(&wait_in_queue_cond[subsequent]);
    pthread_mutex_unlock(&wait_in_queue_mutex[subsequent]);
}

// used to represent a car
void* CarRun(void* arg){
    car_ptr car = (car_ptr)arg;       // copy the argument
    pthread_mutex_lock(&wait_in_queue_mutex[car->direction]);        // lock

    // come and wait
    while(true){
        // if this car is the first one in the waiting queue
        if((car->prev) == &head[car->direction]){
            // enter the crossing (not really entering, i'd like to call it prepared)
            car->prev->next = car->next;
            car->next->prev = car->prev;        // delete node car from the linked list
            car_in_queue[car->direction]--;     // update car in queue number
            break;
        }
        else{
            pthread_cond_wait(&wait_in_queue_cond[car->direction], &wait_in_queue_mutex[car->direction]);       // else, wait in queue
        }
    }

    // enter the crossing
    switch(car->direction){
        case 0:
            Passing(car, 0, 2, 3, 0);       // call Passing to pass
            pthread_exit(NULL);     // exit the thread
            break;
        case 1:
            Passing(car, 3, 1, 2, 1);       // call Passing to pass
            pthread_exit(NULL);     // exit the thread
            break;
        case 2:
            Passing(car, 2, 3, 0, 2);       // call Passing to pass
            pthread_exit(NULL);     // exit the thread
            break;
        case 3:
            Passing(car, 1, 0, 1, 3);       // call Passing to pass
            pthread_exit(NULL);     // exit the thread
            break;
    }
}



int main(){
    pthread_t current_thread_array[MAX_CAR_NUM];        // an array used to store all the thread id of current threads
    // init mutex lock and condition variable
    for(int i = 0; i < 4; i++){
        pthread_mutex_init(&wait_in_queue_mutex[i], NULL);
        pthread_cond_init(&wait_in_queue_cond[i], NULL);
        pthread_mutex_init(&quadrant_mutex[i], NULL);
        pthread_mutex_init(&wait_in_crossing_mutex[i], NULL);
        pthread_cond_init(&wait_in_crossing_cond[i], NULL);
    }
    // init the linked list
    head = (car_ptr)malloc(sizeof(struct car_on_road) * 4);
    tail = (car_ptr)malloc(sizeof(struct car_on_road) * 4);
    // link head and tail
    for(int i = 0; i < 4; i++){
        head[i].next = &tail[i]; head[i].prev = NULL;
        tail[i].next = NULL; tail[i].prev = &head[i];
    }

    srand(time(NULL));      // random seed

    //while(true){
    for(int i = 0; i < MAX_CAR_NUM; i++){
        int return_code;        // capture the return code of pthread_create()
        car_ptr new_car = (car_ptr)malloc(sizeof(struct car_on_road));      // create a new node
        new_car->direction = rand() % 4;      // randomly generate the direction of the next car
        pthread_mutex_lock(&wait_in_queue_mutex[new_car->direction]);       // lock
        new_car->prev = tail[new_car->direction].prev;
        new_car->prev->next = new_car;
        new_car->next = &(tail[new_car->direction]);
        tail[new_car->direction].prev = new_car;        // insert the new node into the linked list
        current_car_num++;      // update current car number
        car_in_queue[new_car->direction]++;     // update car in queue number
        new_car->num = total_car_num++;      // update total car number
        printf("car %d from %s arrives.\n", new_car->num, direction[new_car->direction]);
        return_code = pthread_create(&(new_car->thread_id), NULL, CarRun, (void*)new_car);     // create a new car(thread)

        current_thread_array[i] = new_car->thread_id;       // store the thread id in an array, since the root thread should join them

        // if pthread_create() failed, alert and exit
        if(return_code){
            printf("ERROR! return code %d\n", return_code);
            exit(-1);
        }
        pthread_mutex_unlock(&wait_in_queue_mutex[new_car->direction]);
    }
    // join all of its sub threads
    for(int i = 0; i < MAX_CAR_NUM; i++){
        pthread_join(current_thread_array[i], NULL);
    }
}
