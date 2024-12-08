#include <iostream>
#include <map>
#include <vector>
#include <queue>
#include <fstream>
#include <pthread.h>
#include <sstream>
#include <algorithm>

#include "threadpool.h"

#define NUM_LETTERS 26

using namespace std;

/* Function that transforms words so that they contain only alphabetic characters */
string transform_word(string word) {
    for (size_t i = 0; i < word.size(); i++) {
        if ((word[i] >= 'A') && (word[i] <= 'Z')) {
            /* transforms uppercase characters to lowercase */
            word[i] += 32;
        } else if (!(((word[i] >= 'A') && (word[i] <= 'Z')) ||
                    ((word[i] >= 'a') && (word[i] <= 'z')))) {
            /* delete non-alphabetic characters */
            word.erase(word.begin() + i);
            i--;
        }
    }
    return word;
}

/* Function that extracts all the unique words from a file and adds them in a partial list */
void extract_words_from_file(ifstream& input, unsigned int file_id, map<string, vector<unsigned int>> *partial_list) {
    string current_line;
    while (getline(input, current_line)) {
        string word;
        stringstream words(current_line);
        while (words >> word) {
            string transformed_word = transform_word(word);
            if (!transformed_word.empty()) {
                /* checks if the word is not in the partial list */
                if (partial_list->find(transformed_word) == partial_list->end()) {
                    /* add the word and given file_id in the list */
                    vector<unsigned int> file_ids;
                    file_ids.push_back(file_id);
                    partial_list->insert(make_pair(transformed_word, file_ids));
                } else {
                    /* the list of file ids is not empty */
                    /* if the current file id is not in the file id list of the word, add it */
                    bool found_id = false;
                    for (auto id : partial_list->at(transformed_word)) {
                        if (id == file_id) found_id = true;
                    }
                    if (!found_id) {
                        partial_list->at(transformed_word).push_back(file_id);
                    }
                }
            }
        }
    }
}

void *mapper_function(void *arg) {
    if (arg == NULL) {
        cout << "Argumentul pentru mapper function nu este valid!";
        exit(-1);
    }

    /* extract thread struct data */
    thread_data_t *thread_data = (thread_data_t *) arg;
    threadpool_t *tp = (threadpool_t *) thread_data->threadpool;
    unsigned int thread_id = thread_data->thread_id;

    map<string, vector<unsigned int>> partial_list;
    /* process available files from files queue */
    while (true) {
        pthread_mutex_lock(&tp->work_mutex);
        if (tp->files_queue.empty()) {
            /* the partial list of the thread is copied
             * in the partial lists array */
            tp->partial_lists[thread_id].elems = partial_list;

            pthread_mutex_unlock(&tp->work_mutex);
            pthread_barrier_wait(&tp->mappers_work_barrier);
            pthread_exit(NULL);
        }

        /* extracts the first node in queue */
        file_info_t file_info = tp->files_queue.front();
        tp->files_queue.pop();

        pthread_mutex_unlock(&tp->work_mutex);

        /* process the given file from queue */
        ifstream q_file(file_info.file_path);
        if (!q_file) {
            cerr << "\nNu a putut fi deschis fisierul " << file_info.file_path << " !\n\n";
        }

        /* partial_lists[i] contains the partial list corresponding
         * to the thread with thread_id equal to "i" */
        extract_words_from_file(q_file, file_info.file_id, &partial_list);

        q_file.close();
    }
}

/* Function that handles the processing of the partial list
 * corresponding to the given index argument */
void process_partial_list(threadpool_t *tp, int partial_list_idx) {
    if (tp == NULL) {
        cout << "Argumentul \"tp\" pentru functia \"process_partial_list\" nu este valid!";
        exit(4);
    }
    /* aggregation of words from partial lists to the final lists */
    for (const auto &current_word : tp->partial_lists[partial_list_idx].elems) {
        unsigned int letter_index = current_word.first[0] - 'a';
        int found_word_idx = -1;
        for (unsigned int i = 0; i < tp->final_lists[letter_index].word_list.size(); i++) {
            if (tp->final_lists[letter_index].word_list[i].first == current_word.first) {
                found_word_idx = i;
            }
        }
        if (found_word_idx == -1) {
            /* the word was not found in the final list, add the
             * given word with all file ids to the final list */
            tp->final_lists[letter_index].word_list.push_back(make_pair(current_word.first, current_word.second));
        } else {
            /* add the file ids in the vector corresponding to the
             * found word from the final list */
            tp->final_lists[letter_index].word_list[found_word_idx].second.insert(
                tp->final_lists[letter_index].word_list[found_word_idx].second.end(),
                current_word.second.begin(),
                current_word.second.end()
            );
        }
    }
}

/* Function that writes the files corresponding
 * to all letters of the English alphabet */
void write_output_files(threadpool_t *tp) {
    pthread_mutex_lock(&tp->work_mutex);
    if (!tp->started_writing) {
        /* all reducer threads have finished sorting,
         * now only one thread is doing the file writing */
        tp->started_writing = true;
        pthread_mutex_unlock(&tp->work_mutex);

        /* writing in the corresponding letter files */
        for (unsigned int i = 0; i < NUM_LETTERS; i++) {
            /* open file for writing */
            ofstream letter_file;
            string filename = string(1, i+'a') + ".txt";
            letter_file.open(filename);

            if (!letter_file.is_open()) {
                cerr << "Eroare la deschiderea fisierului: " << filename << endl;
            }

            for (unsigned int j = 0; j < tp->final_lists[i].word_list.size(); j++) {
                string word = tp->final_lists[i].word_list[j].first;
                vector<unsigned int> str_vec = tp->final_lists[i].word_list[j].second;
                /* write the words in the file */
                letter_file << word << ": [";
                for (size_t k = 0; k < str_vec.size(); k++) {
                    if (k == str_vec.size() - 1) {
                        /* no space insertion after last file id */
                        letter_file << str_vec[k];
                    } else {
                        letter_file << str_vec[k] << " ";
                    }
                }                    
                letter_file << "]\n";
            }

            letter_file.close();
        }

        /* signals the completion of the "reduce" stage */
        pthread_mutex_lock(&tp->work_mutex);
        tp->finished_reducing = true;
        pthread_mutex_unlock(&tp->work_mutex);
    } else {
        /* the other reducer threads exit */
        pthread_mutex_unlock(&tp->work_mutex);
        pthread_exit(NULL);
    }
}

void *reducer_function(void *arg) {
    if (arg == NULL) {
        cout << "Argumentul pentru reducer function nu este valid!";
        exit(-1);
    }

    /* extract data from thread_struct */
    thread_data_t *thread_data = (thread_data_t *) arg;
    threadpool_t *tp = (threadpool_t *) thread_data->threadpool;
    unsigned int thread_id = thread_data->thread_id;

    /* wait all mapper threads to finish their work */
    pthread_barrier_wait(&tp->mappers_work_barrier);
    /* process all partial lists obtained by mappers */
    while (true) {
        pthread_mutex_lock(&tp->work_mutex);

        // daca nu mai este de lucru pentru reduceri, thread-ul isi termina executia
        if (tp->finished_reducing) {
            pthread_mutex_unlock(&tp->work_mutex);
            pthread_exit(NULL);
        }

        int unvisited_partial_list = -1;
        // cauta prima lista partiala nevizitata
        for (size_t i = 0; i < tp->num_mapper_threads; i++) {
            if (!(tp->partial_lists[i].is_visited_by_reducer)) {
                // marcheaza lista partiala ca fiind vizitata
                tp->partial_lists[i].is_visited_by_reducer = true;
                unvisited_partial_list = i;
                break;
            }
        }
        if (unvisited_partial_list != -1) {
            // proceseaza lista partiala nevizitata data si o pune in lista finala
            process_partial_list(tp, unvisited_partial_list);
            pthread_mutex_unlock(&tp->work_mutex);
        } else {
            // au fost vizitate toate listele partiale
            tp->finished_aggregation[thread_id - tp->num_mapper_threads] = true;
            bool all_reducers_finished_aggregation = true;
            for (size_t i = 0; i < tp->num_reducer_threads; i++) {
                if (!tp->finished_aggregation[i]) {
                    all_reducers_finished_aggregation = false;
                    break;
                }
            }
            pthread_mutex_unlock(&tp->work_mutex);
            if (all_reducers_finished_aggregation) {
                pthread_mutex_lock(&tp->work_mutex);
                int unvisited_final_list = -1;
                // cauta prima lista finala nevizitata
                for (size_t i = 0; i < NUM_LETTERS; i++) {
                    if (!(tp->final_lists[i].is_visited_by_reducer)) {
                        // marcheaza lista finala ca fiind vizitata
                        tp->final_lists[i].is_visited_by_reducer = true;
                        unvisited_final_list = i;
                        break;
                    }
                }
                pthread_mutex_unlock(&tp->work_mutex);
                if (unvisited_final_list != -1) {
                    // sortare lista finala corespunzatoare literei de la index-ul "unvisited_final_list"
                    // sortare crescatoare a file_id-urilor fiecarui cuvant
                    for (unsigned int j = 0; j < tp->final_lists[unvisited_final_list].word_list.size(); j++) {
                        sort(tp->final_lists[unvisited_final_list].word_list[j].second.begin(),
                             tp->final_lists[unvisited_final_list].word_list[j].second.end());
                    }
                    // sortarea cuvintelor care incep cu litera de la index-ul "unvisited_final_list"
                    sort(tp->final_lists[unvisited_final_list].word_list.begin(),
                         tp->final_lists[unvisited_final_list].word_list.end(),
                         sort_elements());
                } else {
                    pthread_mutex_lock(&tp->work_mutex);
                    tp->reducers_finished_sorting[thread_id] = true;
                    bool all_reducers_finished_sorting = true;
                    for (size_t i = 0; i < tp->num_reducer_threads; i++) {
                        if (!tp->reducers_finished_sorting[i]) {
                            all_reducers_finished_aggregation = false;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&tp->work_mutex);
                    if (all_reducers_finished_sorting) {
                        write_output_files(tp);
                    }
                }
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        cout << "\nUtilizare: <numar_mapperi> <numar_reduceri> <fisier_intrare>\n\n";
        exit(0);
    }

    long int num_mapper_threads;
    long int num_reducer_threads;

    // se stocheaza argumentul <numar_mapperi>, daca este valid
    try {
        num_mapper_threads = stol(argv[1]);
        if (num_mapper_threads <= 0) {
            throw std::invalid_argument("Numar de mapperi negativ");
        }
    } catch (const std::invalid_argument& e) {
        cerr << "\nNumarul de mapperi nu este valid!\n\n";
        exit(1);
    }

    // se stocheaza argumentul <numar_reduceri>, daca este valid
    try {
        num_reducer_threads = stol(argv[2]);
        if (num_reducer_threads <= 0) {
            throw std::invalid_argument("Numar de reduceri negativ");
        }
    } catch (const std::invalid_argument& e) {
        cerr << "\nNumarul de reduceri nu este valid!\n\n";
        exit(2);
    }

    // se deschide fisierul, daca este valid
    ifstream fin(argv[3]);
    if (!fin) {
        cerr << "\nNu a putut fi deschis fisierul!\n\n";
        exit(3);
    }

    // citire numar fisiere
    string line;
    getline(fin, line);
    unsigned long files_num = stoul(line);

    // initializare threadpool
    threadpool_t tp;
    tp.num_mapper_threads = num_mapper_threads;
    tp.num_reducer_threads = num_reducer_threads;
    tp.threads = (pthread_t*) calloc(num_mapper_threads + num_reducer_threads, sizeof(pthread_t));
    pthread_mutex_init(&(tp.work_mutex), NULL);
    tp.partial_lists = new partial_list_t[num_mapper_threads];
    tp.final_lists = new final_list_t[NUM_LETTERS];
    pthread_barrier_init(&(tp.mappers_work_barrier), NULL, num_mapper_threads + num_reducer_threads);
    tp.started_writing = false;
    tp.finished_reducing = false;
    tp.finished_aggregation = (bool*) calloc(num_reducer_threads, sizeof(bool));
    tp.reducers_finished_sorting = (bool*) calloc(num_reducer_threads, sizeof(bool));

    // initializare valoare bool a listelor partiale
    for (size_t i = 0; i < tp.num_mapper_threads; i++) {
        tp.partial_lists[i].is_visited_by_reducer = false;
    }

    // initializare valoare bool a listelor finale
    for (size_t i = 0; i < NUM_LETTERS; i++) {
        tp.final_lists[i].is_visited_by_reducer = false;
    }

    // adaugarea datelor fisierelor in coada
    for (size_t i = 1; i <= files_num; i++) {
        getline(fin, line);
        file_info_t file_info;
        file_info.file_id = i;
        file_info.file_path = line;
        tp.files_queue.push(file_info);
    }

    // creare coada pentru detaliile thread-urilor
    queue<thread_struct_t*> threads_structs;
    for (long int i = 0; i < num_mapper_threads + num_reducer_threads; i++) {
        thread_struct_t *thread_struct= new thread_struct_t;
        thread_struct->data.thread_id = i;
        thread_struct->data.threadpool = &tp;
        if (i < num_mapper_threads) {
            thread_struct->function = mapper_function;
        } else {
            thread_struct->function = reducer_function;
        }
        threads_structs.push(thread_struct);
    }

    // se initializeaza toate thread-urile
    for (long int i = 0; !threads_structs.empty(); i++) {
        int ret = pthread_create(&tp.threads[i], NULL, threads_structs.front()->function, &threads_structs.front()->data);
        if (ret) {
            cout << "Eroare la crearea thread-ului " << i << "\n";
            exit(-1);
        }
        threads_structs.pop();
    }

    // se asteapta toate thread-urile
    for (int i = 0; i < num_mapper_threads + num_reducer_threads; i++) {
        void *status;
        int ret = pthread_join(tp.threads[i], &status);
        if (ret) {
            cout << "Eroare la asteptarea thread-ului " << i << "\n";
            exit(-1);
        }
    }

    // distrugere mutex din threadpool
    pthread_mutex_destroy(&(tp.work_mutex));
    pthread_barrier_destroy(&(tp.mappers_work_barrier));

    // se elibereaza memoria alocata pentru thread-uri si pentru liste
    free(tp.threads);
    delete[] tp.partial_lists;

    return 0;
}
