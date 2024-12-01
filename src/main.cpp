#include <iostream>
#include <map>
#include <vector>
#include <queue>
#include <fstream>
#include <pthread.h>

#include "threadpool.h"

using namespace std;

void *mapper_function(void *arg) {
    cout << "New thread:\n";
    if (arg == NULL) {
        cout << "Argumentul pentru mapper function nu este valid!";
        exit(-1);
    }
    threadpool_t *tp = (threadpool_t *) arg;
    // procesarea fisierelor disponibile in coada
    while (true) {
        pthread_mutex_lock(&tp->work_mutex);

        // daca e setat flagul de "finished_mapping", thread-ul se opreste
        if (tp->finished_mapping) {
            pthread_mutex_unlock(&tp->work_mutex);
            pthread_exit(NULL);
        }

        // in coada nu mai sunt fisiere de procesat
        if (tp->files_queue.empty()) {
            tp->finished_mapping = true;
            pthread_mutex_unlock(&tp->work_mutex);
            pthread_exit(NULL);
        }

        // extrage din coada primul fisier disponibil
        file_info_t file_info = tp->files_queue.front();
        tp->files_queue.pop();

        pthread_mutex_unlock(&tp->work_mutex);

        // proceseaza fisierul primit din coada
        ifstream q_file(file_info.file_path);
        if (!q_file) {
            cerr << "\nNu a putut fi deschis fisierul " << file_info.file_path << " !\n\n";
        }

        cout << "Fisierul primit este: " << file_info.file_path << "\n Continutul fisierului:\n";

        string current_line;
        while (getline(q_file, current_line)) {
            cout << current_line << "\n";
        }

        extract_words_from_file(q_file, &(tp->partial_lists[]));

        // inchidere fisier
        q_file.close();
    }
}

void *reducer_function(void *arg) {
    threadpool_t *tp = (threadpool_t *) arg;
    if (tp == NULL) {
        cout << "Argumentul pentru reducer function nu este valid!";
        exit(-1);
    }

    // declarare lista finala generata de thread-ul curent
    // aceasta contine elemente de tipul (cuvant, lista de indecsi ai fisierelor)
    map<string, vector<unsigned int>> thread_list;

    // procesare task-uri
    while (true) {
        pthread_mutex_lock(&tp->work_mutex);

        // daca nu mai este de lucru pentru reduceri, thread-ul isi termina executia
        if (tp->finished_reducing) {
            pthread_mutex_unlock(&tp->work_mutex);
            pthread_exit(NULL);
        }
        if (tp->finished_aggregation) {
            // TODO: primul thread disponibil se ocupa de scrierea fisierelor "litera".txt

            tp->finished_reducing = true;
            pthread_mutex_unlock(&tp->work_mutex);
            pthread_exit(NULL);
        }

        // verifica daca mapperii au terminat lucrul
        if (tp->finished_mapping) {
            // agregarea cuvintelor din listele partiale disponibile in lista thread-ului curent

            // cauta prima lista nevizitata
            for (size_t i = 0; i < tp->partial_lists.size(); i++) {
                if (!(tp->partial_lists[i].is_visited)) {
                    // TODO
                    // process given unvisited partial list and put it in the thread list
                }
            }
            
            pthread_mutex_unlock(&tp->work_mutex);
            pthread_exit(NULL);
        }

        pthread_mutex_unlock(&tp->work_mutex);
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

    cout << "Numarul de mapper threads: " << num_mapper_threads << "\n";
    cout << "Numarul de reducer threads: " << num_reducer_threads << "\n";
    cout << "Fisierul citit: " << argv[3] << "\n";

    // initializare threadpool
    threadpool_t tp;
    tp.finished_mapping = false;
    tp.finished_reducing = false;
    pthread_mutex_init(&(tp.work_mutex), NULL);
    tp.threads = (pthread_t*) calloc(num_mapper_threads + num_reducer_threads, sizeof(pthread_t));
    // tp.partial_lists = (map<string, vector<unsigned int>>*) calloc(num_mapper_threads, sizeof(map<string, vector<unsigned int>>));

    // citire numar fisiere
    string line;
    getline(fin, line);
    unsigned long files_num = stoul(line);

    cout << "Numarul de fisiere: " << files_num << "\n";
    
    for (size_t i = 1; getline(fin, line); i++) {
        cout << "se baga in coada linia: " << line << ", id=" << i << "\n";
        file_info_t file_info;
        file_info.file_id = i;
        file_info.file_path = line;
        tp.files_queue.push(file_info);
    }

    // while (!tp.files_queue.empty()) {
       
    //     // getting front element of queue
    //     cout << tp.files_queue.front().file_path << " ";
       
    //     // removing front element of queue
    //     tp.files_queue.pop();
    // }

    // se initializeaza thread-urile "mapper"
    for (long int i = 0; i < num_mapper_threads; i++) {
        int ret = pthread_create(&tp.threads[i], NULL, mapper_function, &tp);
        if (ret) {
            cout << "Eroare la crearea thread-ului " << i << "\n";
            exit(-1);
        }
    }

    
    

    // se initializeaza thread-urile "reducer"
    // for (long int i = num_mapper_threads; i < num_mapper_threads + num_reducer_threads; i++) {
    //     int ret = pthread_create(&tp.threads[i], NULL, reducer_function, &tp);
    //     if (ret) {
    //         cout << "Eroare la crearea thread-ului " << i << "\n";
    //         exit(-1);
    //     }
    // }

    cout << "Se asteapta toate thread-urile...\n";

    // se asteapta toate thread-urile
    for (int i = 0; i < num_mapper_threads; i++) {
        void *status;
        int ret = pthread_join(tp.threads[i], &status);
        if (ret) {
            cout << "Eroare la asteptarea thread-ului " << i << "\n";
            exit(-1);
        }
    }

    // exit(10);

    // distrugere mutex din threadpool
    pthread_mutex_destroy(&(tp.work_mutex));

    // se elibereaza memoria alocata pentru thread-uri si pentru liste
    free(tp.threads);
    // free(tp.partial_lists);

    return 0;
}
