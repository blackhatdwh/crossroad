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
#define EAST 0
#define WEST 1
#define SOUTH 2
#define NORTH 3

// 0: NORTH  1: SOUTH  2: WEST  3: EAST
char* direction[4] = {"NORTH", "SOUTH", "WEST", "EAST"};

// a linked list is used to store pthread_t of those currently alive threads
// one thread represent one car. one thread in this linked list represent a car on the road
typedef struct car_on_road* car_ptr;
struct car_on_road {
    pthread_t thread_id;
    int direction;
    int num;
    car_ptr next;
    car_ptr prev;
};
car_ptr head;
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

pthread_cond_t wait_in_crossing_cond[4];
pthread_mutex_t wait_in_crossing_mutex[4];

pthread_mutex_t deadlock_detect_mutex = PTHREAD_MUTEX_INITIALIZER;      // ensure that no more than one detect routine can be running


int total_car_num = 0;      // indicates how many cars have been created since the program started
int current_car_num = 0;        // indicates how many cars are on the road currently
int car_in_queue[4] = {0, 0, 0, 0};       // indicates the number of cars in queue in each direction

void* DeadLockDetect(void* arg){
    pthread_mutex_lock(&deadlock_detect_mutex);
    int deadlock_flag = 1;
    for(int i = 0; i < 4; i++){
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
        printf("DEADLOCK: car jam detected, signalling North to go.\n");
        usleep(8000);

        // signal the north car to go first
        pthread_mutex_lock(&wait_in_crossing_mutex[0]);
        pthread_cond_signal(&wait_in_crossing_cond[0]);
        pthread_mutex_unlock(&wait_in_crossing_mutex[0]);
    }
    pthread_mutex_unlock(&deadlock_detect_mutex);
}

void* CarRun(void* arg){
    car_ptr car = (car_ptr)arg;       // copy the argument
    pthread_mutex_lock(&wait_in_queue_mutex[car->direction]);        // lock

    // come and wait
    while(true){
        // if this car is the first one in the waiting queue
        if((car->prev) == &head[car->direction]){
            // enter the crossing
            car->prev->next = car->next;
            car->next->prev = car->prev;        // delete node car from the linked list
            car_in_queue[car->direction]--;     // update car in queue number
            break;
        }
        else{
            pthread_cond_wait(&wait_in_queue_cond[car->direction], &wait_in_queue_mutex[car->direction]);       // else, wait in queue
        }
    }

    int return_code;
    pthread_t deadlock_thread_id;
    // enter the crossing
    switch(car->direction){
        case 0:
            pthread_mutex_lock(&quadrant_mutex[0]);
            // start the DeadLockDetection routine
            pthread_create(&deadlock_thread_id, NULL, DeadLockDetect, NULL);

            usleep(5000);       // observe around before go through the crossing
            return_code = pthread_mutex_trylock(&quadrant_mutex[2]);
            // successfully locked the second mutex, so it can go directly
            if(return_code == 0){
                printf("car %d from %s leaves.\n", car->num, direction[car->direction]);
                // unlock its own two quadrants
                pthread_mutex_unlock(&quadrant_mutex[0]);
                pthread_mutex_unlock(&quadrant_mutex[2]);
                // notify its left car to go
                pthread_mutex_lock(&wait_in_crossing_mutex[3]);
                pthread_cond_signal(&wait_in_crossing_cond[3]);
                pthread_mutex_unlock(&wait_in_crossing_mutex[3]);
                usleep(5000);       // sleep 5ms to let left car go first
                // notify its subsequent cars to move forward once
                pthread_cond_broadcast(&wait_in_queue_cond[0]);
                pthread_mutex_unlock(&wait_in_queue_mutex[0]);
                pthread_exit(NULL);
            }
            // else, wait for signal. signal can be produced by DeadLockDetection or right car
            else{
                pthread_mutex_lock(&wait_in_crossing_mutex[car->direction]);
                pthread_cond_wait(&wait_in_crossing_cond[car->direction], &wait_in_crossing_mutex[car->direction]);
                pthread_mutex_unlock(&wait_in_crossing_mutex[car->direction]);
            }
            printf("car %d from %s leaves.\n", car->num, direction[car->direction]);     // when signaled, go directly
            pthread_mutex_unlock(&quadrant_mutex[0]);       // unlock its own one quadrants
            // notify its left car to go
            pthread_mutex_lock(&wait_in_crossing_mutex[3]);
            pthread_cond_signal(&wait_in_crossing_cond[3]);
            pthread_mutex_unlock(&wait_in_crossing_mutex[3]);
            usleep(5000);       // sleep 5ms to let left car go first
            // notify its subsequent cars to move forward once
            pthread_cond_broadcast(&wait_in_queue_cond[0]);
            pthread_mutex_unlock(&wait_in_queue_mutex[0]);
            pthread_exit(NULL);
            break;
        case 1:
            pthread_mutex_lock(&quadrant_mutex[3]);
            // start the DeadLockDetection routine
            pthread_create(&deadlock_thread_id, NULL, DeadLockDetect, NULL);
            usleep(5000);
            return_code = pthread_mutex_trylock(&quadrant_mutex[1]);
            // successfully locked the second mutex, so it can go directly
            if(return_code == 0){
                printf("car %d from %s leaves.\n", car->num, direction[car->direction]);
                // unlock its own two quadrants
                pthread_mutex_unlock(&quadrant_mutex[3]);
                pthread_mutex_unlock(&quadrant_mutex[1]);
                // notify its left car to go
                pthread_mutex_lock(&wait_in_crossing_mutex[2]);
                pthread_cond_signal(&wait_in_crossing_cond[2]);
                pthread_mutex_unlock(&wait_in_crossing_mutex[2]);
                usleep(5000);       // sleep 5ms to let left car go first
                // notify its subsequent cars to move forward once
                pthread_cond_broadcast(&wait_in_queue_cond[1]);
                pthread_mutex_unlock(&wait_in_queue_mutex[1]);
                pthread_exit(NULL);
            }
            // else, wait for signal. signal can be produced by DeadLockDetection or right car
            else{
                pthread_mutex_lock(&wait_in_crossing_mutex[car->direction]);
                pthread_cond_wait(&wait_in_crossing_cond[car->direction], &wait_in_crossing_mutex[car->direction]);
                pthread_mutex_unlock(&wait_in_crossing_mutex[car->direction]);
            }
            printf("car %d from %s leaves.\n", car->num, direction[car->direction]);     // when signaled, go directly
            pthread_mutex_unlock(&quadrant_mutex[3]);       // unlock its own one quadrants
            // notify its left car to go
            pthread_mutex_lock(&wait_in_crossing_mutex[2]);
            pthread_cond_signal(&wait_in_crossing_cond[2]);
            pthread_mutex_unlock(&wait_in_crossing_mutex[2]);
            usleep(5000);       // sleep 5ms to let left car go first
            // notify its subsequent cars to move forward once
            pthread_cond_broadcast(&wait_in_queue_cond[1]);
            pthread_mutex_unlock(&wait_in_queue_mutex[1]);
            pthread_exit(NULL);
            break;
        case 2:
            pthread_mutex_lock(&quadrant_mutex[2]);
            // start the DeadLockDetection routine
            pthread_create(&deadlock_thread_id, NULL, DeadLockDetect, NULL);
            usleep(5000);
            return_code = pthread_mutex_trylock(&quadrant_mutex[3]);
            // successfully locked the second mutex, so it can go directly
            if(return_code == 0){
                printf("car %d from %s leaves.\n", car->num, direction[car->direction]);
                // unlock its own two quadrants
                pthread_mutex_unlock(&quadrant_mutex[2]);
                pthread_mutex_unlock(&quadrant_mutex[3]);
                // notify its left car to go
                pthread_mutex_lock(&wait_in_crossing_mutex[0]);
                pthread_cond_signal(&wait_in_crossing_cond[0]);
                pthread_mutex_unlock(&wait_in_crossing_mutex[0]);
                usleep(5000);       // sleep 5ms to let left car go first
                // notify its subsequent cars to move forward once
                pthread_cond_broadcast(&wait_in_queue_cond[2]);
                pthread_mutex_unlock(&wait_in_queue_mutex[2]);
                pthread_exit(NULL);
            }
            // else, wait for signal. signal can be produced by DeadLockDetection or right car
            else{
                pthread_mutex_lock(&wait_in_crossing_mutex[car->direction]);
                pthread_cond_wait(&wait_in_crossing_cond[car->direction], &wait_in_crossing_mutex[car->direction]);
                pthread_mutex_unlock(&wait_in_crossing_mutex[car->direction]);
            }
            printf("car %d from %s leaves.\n", car->num, direction[car->direction]);     // when signaled, go directly
            pthread_mutex_unlock(&quadrant_mutex[2]);       // unlock its own one quadrants
            // notify its left car to go
            pthread_mutex_lock(&wait_in_crossing_mutex[0]);
            pthread_cond_signal(&wait_in_crossing_cond[0]);
            pthread_mutex_unlock(&wait_in_crossing_mutex[0]);
            usleep(5000);       // sleep 5ms to let left car go first
            // notify its subsequent cars to move forward once
            pthread_cond_broadcast(&wait_in_queue_cond[2]);
            pthread_mutex_unlock(&wait_in_queue_mutex[2]);
            pthread_exit(NULL);
            break;
        case 3:
            pthread_mutex_lock(&quadrant_mutex[1]);
            // start the DeadLockDetection routine
            pthread_create(&deadlock_thread_id, NULL, DeadLockDetect, NULL);
            usleep(5000);
            return_code = pthread_mutex_trylock(&quadrant_mutex[0]);
            // successfully locked the second mutex, so it can go directly
            if(return_code == 0){
                printf("car %d from %s leaves.\n", car->num, direction[car->direction]);
                // unlock its own two quadrants
                pthread_mutex_unlock(&quadrant_mutex[1]);
                pthread_mutex_unlock(&quadrant_mutex[0]);
                // notify its left car to go
                pthread_mutex_lock(&wait_in_crossing_mutex[1]);
                pthread_cond_signal(&wait_in_crossing_cond[1]);
                pthread_mutex_unlock(&wait_in_crossing_mutex[1]);
                usleep(5000);       // sleep 5ms to let left car go first
                // notify its subsequent cars to move forward once
                pthread_cond_broadcast(&wait_in_queue_cond[3]);
                pthread_mutex_unlock(&wait_in_queue_mutex[3]);
                pthread_exit(NULL);
            }
            // else, wait for signal. signal can be produced by DeadLockDetection or right car
            else{
                pthread_mutex_lock(&wait_in_crossing_mutex[car->direction]);
                pthread_cond_wait(&wait_in_crossing_cond[car->direction], &wait_in_crossing_mutex[car->direction]);
                pthread_mutex_unlock(&wait_in_crossing_mutex[car->direction]);
            }
            printf("car %d from %s leaves.\n", car->num, direction[car->direction]);     // when signaled, go directly
            pthread_mutex_unlock(&quadrant_mutex[1]);       // unlock its own one quadrants
            // notify its left car to go
            pthread_mutex_lock(&wait_in_crossing_mutex[1]);
            pthread_cond_signal(&wait_in_crossing_cond[1]);
            pthread_mutex_unlock(&wait_in_crossing_mutex[1]);
            usleep(5000);       // sleep 5ms to let left car go first
            // notify its subsequent cars to move forward once
            pthread_cond_broadcast(&wait_in_queue_cond[3]);
            pthread_mutex_unlock(&wait_in_queue_mutex[3]);
            pthread_exit(NULL);
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
        // new_car->direction = i % 4;
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

        current_thread_array[i] = new_car->thread_id;

        // if pthread_create() failed, alert and exit
        if(return_code){
            printf("ERROR! return code %d\n", return_code);
            exit(-1);
        }
        pthread_mutex_unlock(&wait_in_queue_mutex[new_car->direction]);
    }
    for(int i = 0; i < MAX_CAR_NUM; i++){
        pthread_join(current_thread_array[i], NULL);
    }
}
