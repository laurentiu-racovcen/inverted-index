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
    map<string, vector<unsigned int>> elems;
    bool is_visited_by_reducer;
} partial_list_t;

typedef struct {
    vector<pair<string, vector<unsigned int>>> word_list;
    bool is_visited_by_reducer;
} final_list_t;

typedef struct {
    unsigned int file_id;
    string file_path;
} file_info_t;

typedef struct {
    unsigned int num_mapper_threads;
    unsigned int num_reducer_threads;
    pthread_t *threads;
    pthread_mutex_t work_mutex;         // pentru sincronizarea operatiilor efectuate de thread-uri
    queue<file_info_t> files_queue;     // stocheaza datele fisierelor care trebuie procesate de mapperi
    pthread_cond_t cond;
    partial_list_t* partial_lists;
    final_list_t* final_lists;
    pthread_barrier_t mappers_work_barrier; // pentru ca reducerii sa astepte mapperii sa isi termine lucrul
    bool started_writing;                   // pentru a semnaliza daca un thread a inceput scrierea in fisiere
    bool finished_reducing;                 // pentru a semnaliza daca procesarea de catre reduceri s-a terminat
    bool *finished_aggregation;
    bool *reducers_finished_sorting;
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
