#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <iostream>
#include <pthread.h>
#include <string>
#include <queue>

using namespace std;

/* For sorting the final list elements */
struct sort_elements {
    bool operator() (const pair<string, vector<unsigned int>> &a,
                    const pair<string, vector<unsigned int>> &b) const {
        /* descending sort by the number of files containing the word */
        if (a.second.size() != b.second.size()) {
            return a.second.size() > b.second.size();
        }
        /* sort alphabetically ascending by word */
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
    unsigned int num_mapper_threads;        // number of mapper threads
    unsigned int num_reducer_threads;       // number of reducer threads
    pthread_t *threads;                     // array storing all created threads
    pthread_mutex_t work_mutex;             // to synchronize thread operations
    queue<file_info_t> files_queue;         // store the data of the files to be processed by the mappers
    partial_list_t* partial_lists;          // partial_lists[i] is the partial list of mapper thread "i"
    final_list_t* final_lists;              // final_lists[i] is the final list of reducer thread "i"
    pthread_barrier_t mappers_work_barrier; // for the reducers to wait for the mappers to finish their work
    bool started_writing;                   // to signal if a thread has started writing to files
    bool finished_reducing;                 // to signal if the processing by the reducer threads has finished
    bool *finished_aggregation;             // finished_aggregation[i] tells if the reducer thread "i" has finished aggregating
    bool *reducers_finished_sorting;        // reducers_finished_sorting[i] tells if reducer thread "i" has finished sorting
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
