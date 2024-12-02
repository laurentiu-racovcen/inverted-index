#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <iostream>
#include <pthread.h>
#include <string>
#include <queue>
#include <set>

using namespace std;

typedef struct {
    map<string, vector<unsigned int>> elems;
    bool is_visited_by_reducer; // pentru sincronizarea thread-urilor reducer
} partial_list_t;

typedef struct {
    unsigned int file_id;
    string file_path;
} file_info_t;

typedef struct {
    unsigned int num_mapper_threads;
    unsigned int num_reducer_threads;
    pthread_t *threads;
    pthread_mutex_t work_mutex;       // pentru sincronizarea operatiilor efectuate de thread-uri
    queue<file_info_t> files_queue;   // stocheaza datele fisierelor care trebuie procesate de mapperi
    pthread_cond_t cond;
    partial_list_t* partial_lists;    // o lista partiala contine elemente de forma (cuvant, lista de file_id-uri)
    bool finished_mapping;            // pentru a semnaliza daca procesarea de catre mapperi s-a terminat (coada de task-uri "map" este goala)
    bool finished_reducing;           // pentru a semnaliza daca procesarea de catre reduceri s-a terminat (coada de task-uri "reduce" este goala)
    bool finished_aggregation;
} threadpool_t;

typedef struct {
    threadpool_t* threadpool;
    unsigned int thread_id;
} thread_struct_t;

#endif // __THREADPOOL_H__
