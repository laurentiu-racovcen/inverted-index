#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <iostream>
#include <pthread.h>
#include <string>
#include <queue>

using namespace std;

struct sort_elements {
    // Functie care sorteaza elementele listei finale
    bool operator() (const pair<string, vector<unsigned int>> &a,
                    const pair<string, vector<unsigned int>> &b) const {
        // sortare descrescatoare
        // dupa numarul de fisiere in care este prezent cuvantul
        if (a.second.size() != b.second.size()) {
            return a.second.size() > b.second.size();
        }
        // sortare crescatoare alfabetica dupa cuvant
        return a.first < b.first;
    }
};

typedef struct {
    map<string, vector<unsigned int>> elems; // lista de forma (cuvant, file_id list)
    bool is_visited_by_reducer; // pentru sincronizarea thread-urilor reducer
} partial_list_t;

typedef vector<pair<string, vector<unsigned int>>> final_list_t;

typedef struct {
    unsigned int file_id;
    string file_path;
} file_info_t;

typedef struct {
    unsigned int num_mapper_threads;
    unsigned int num_reducer_threads;
    pthread_t *threads;
    pthread_mutex_t work_mutex;        // pentru sincronizarea operatiilor efectuate de thread-uri
    queue<file_info_t> files_queue;    // stocheaza datele fisierelor care trebuie procesate de mapperi
    pthread_cond_t cond;
    partial_list_t* partial_lists;
    final_list_t* final_lists;
    unsigned int num_finished_mapping; // pentru a semnaliza daca procesarea de catre mapperi s-a terminat
    bool started_writing;              // pentru a semnaliza daca un thread a inceput scrierea in fisiere
    bool finished_reducing;            // pentru a semnaliza daca procesarea de catre reduceri s-a terminat
    unsigned int num_finished_aggregation;
} threadpool_t;

typedef struct {
    threadpool_t* threadpool;
    unsigned int thread_id;
} thread_data_t;

typedef struct {
    thread_data_t data;
    void* (*function)(void*);
} thread_struct_t;

#endif // __THREADPOOL_H__
