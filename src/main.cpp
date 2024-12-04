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

// Functie care transforma cuvintele astfel incat acestea sa contina
// doar caractere alfabetice
string normalize_word(string word) {
    // cout << "before: " << word << "\n";
    for (size_t i = 0; i < word.size(); i++) {
        if ((word[i] >= 'A') && (word[i] <= 'Z')) {
            // transforma caracterele uppercase in caractere lowercase
            word[i] += 32;
        } else if (!(((word[i] >= 'A') && (word[i] <= 'Z')) ||
                    ((word[i] >= 'a') && (word[i] <= 'z')))) {
            // stergere caractere neconforme (diferite de cele alfabetice)
            word.erase(word.begin() + i);
            i--;
        }
    }
    return word;
}

// Functie care extrage toate cuvintele unice dintr-un fisier
void extract_words_from_file(ifstream& input, unsigned int file_id, map<string, vector<unsigned int>> *partial_list) {
    string current_line;
    while (getline(input, current_line)) {
        string word;
        stringstream words(current_line);
        while (words >> word) {
            string normalized_word = normalize_word(word);
            if (!normalized_word.empty()) {
                // verifica daca cuvantul nu se afla in lista partiala
                if (partial_list->find(normalized_word) == partial_list->end()) {
                    // adauga cuvantul si file_id-ul primit in lista
                    vector<unsigned int> file_ids;
                    file_ids.push_back(file_id);
                    partial_list->insert(make_pair(normalized_word, file_ids));
                } else {
                    // lista de file_id-uri nu este goala;
                    // daca file_id-ul curent nu exista in lista de file_id-uri ale cuvantului, acesta se adauga
                    bool found_id = false;
                    for (auto id : partial_list->at(normalized_word)) {
                        if (id == file_id) found_id = true;
                    }
                    if (!found_id) {
                        partial_list->at(normalized_word).push_back(file_id);
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

    // extragere date din thread_struct
    thread_struct_t *thread_struct = (thread_struct_t *) arg;
    threadpool_t *tp = (threadpool_t *) thread_struct->threadpool;
    unsigned int thread_id = thread_struct->thread_id;

    map<string, vector<unsigned int>> partial_list;
    // procesarea fisierelor disponibile in coada
    while (true) {
        pthread_mutex_lock(&tp->work_mutex);

        // daca numarul de "finished_mapping" este egal cu nr de thread-uri mapper, thread-ul se opreste
        if (tp->num_finished_mapping == tp->num_mapper_threads) {
            pthread_mutex_unlock(&tp->work_mutex);
            pthread_exit(NULL);
        }

        // daca in coada nu mai sunt fisiere de procesat
        if (tp->files_queue.empty()) {
            // lista partiala a thread-ului se copiaza in lista de liste partiale ale thread-urilor
            tp->partial_lists[thread_id].elems = partial_list;
            tp->num_finished_mapping++;

            pthread_mutex_unlock(&tp->work_mutex);
            pthread_exit(NULL);
        }

        // extrage din coada primul nod
        file_info_t file_info = tp->files_queue.front();
        tp->files_queue.pop();

        pthread_mutex_unlock(&tp->work_mutex);

        // proceseaza fisierul primit din coada
        ifstream q_file(file_info.file_path);
        if (!q_file) {
            cerr << "\nNu a putut fi deschis fisierul " << file_info.file_path << " !\n\n";
        }

        // lista partial_lists[i] contine lista partiala corespunzatoare thread-ului cu id-ul "i"
        extract_words_from_file(q_file, file_info.file_id, &partial_list);

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

    // declarare lista finala generata de thread-ul curent pentru fiecare litera
    // aceasta contine elemente de tipul (cuvant, lista de indecsi ai fisierelor)
    vector<vector<pair<string, vector<unsigned int>>>> thread_final_list(NUM_LETTERS);

    // procesare task-uri
    while (true) {
        pthread_mutex_lock(&tp->work_mutex);

        // daca nu mai este de lucru pentru reduceri, thread-ul isi termina executia
        if (tp->finished_reducing) {
            pthread_mutex_unlock(&tp->work_mutex);
            pthread_exit(NULL);
        }

        if ((tp->num_finished_aggregation == tp->num_reducer_threads) && !tp->started_writing) {
            // toate thread-urile reducer au terminat agregarea,
            // acum un singur thread se ocupa de scrierea in fisiere

            tp->started_writing = true;
            pthread_mutex_unlock(&tp->work_mutex);

            // sortare lista finala
            for (unsigned int i = 0; i < NUM_LETTERS; i++) {
                // sortare crescatoare a file_id-urilor fiecarui cuvant
                for (unsigned int j = 0; j < tp->final_lists[i].size(); j++) {
                    sort(tp->final_lists[i][j].second.begin(), tp->final_lists[i][j].second.end());
                    vector<unsigned int> str_vec = tp->final_lists[i][j].second;
                }
                // sortare linii in fisier
                sort(tp->final_lists[i].begin(), tp->final_lists[i].end(), sort_elements());
            }

            // scriere in fisiere
            for (unsigned int i = 0; i < NUM_LETTERS; i++) {
                // deschidere fisier pentru scriere
                ofstream letter_file;
                string filename = string(1, i+'a') + ".txt";
                letter_file.open(filename);

                if (!letter_file.is_open()) {
                    std::cerr << "Error opening file: " << filename << std::endl;
                }

                for (unsigned int j = 0; j < tp->final_lists[i].size(); j++) {
                    string word = tp->final_lists[i][j].first;
                    vector<unsigned int> str_vec = tp->final_lists[i][j].second;
                    // scrie cuvintele in fisier
                    letter_file << word << ": [";
                    for (size_t k = 0; k < str_vec.size(); k++){
                        if (k == str_vec.size() - 1) {
                            // no space char after last file id
                            letter_file << str_vec[k];
                        } else {
                            letter_file << str_vec[k] << " ";
                        }
                    }                    
                    letter_file << "]\n";
                }

                letter_file.close();
            }

            // semnalizeaza terminarea operatiei "reduce" catre toate thread-urile reducer
            pthread_mutex_lock(&tp->work_mutex);
            tp->finished_reducing = true;
            pthread_mutex_unlock(&tp->work_mutex);
            pthread_exit(NULL);
        }

        // verifica daca mapperii au terminat lucrul
        if ((tp->num_finished_mapping == tp->num_mapper_threads) && (tp->num_finished_aggregation != tp->num_reducer_threads)) {
            // agregarea cuvintelor din listele partiale disponibile in lista thread-ului curent
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
                for (const auto &current_word : tp->partial_lists[unvisited_partial_list].elems) {
                    unsigned int letter_index = current_word.first[0] - 'a';
                    thread_final_list[letter_index].push_back(make_pair(current_word.first, current_word.second));

                    // agregarea cuvintelor in lista finala
                    // listele partiale contin doar indici diferiti ai file_id-urilor, deci nu e nevoie sa verificam existenta duplicatelor
                    int found_word_idx = -1;

                    for (unsigned int i = 0; i < tp->final_lists[letter_index].size(); i++) {
                        if (tp->final_lists[letter_index][i].first == current_word.first) {
                            found_word_idx = i;
                        }
                    }

                    if (found_word_idx == -1) {
                        // cuvantul nu a fost gasit in lista finala
                        // adauga cuvantul dat cu file_id-uri in lista finala
                        tp->final_lists[letter_index].push_back(make_pair(current_word.first, current_word.second));
                    } else {
                        // adauga id-urile gasite in vectorul corespunzator listei literei din lista finala
                        tp->final_lists[letter_index][found_word_idx].second.insert(tp->final_lists[letter_index][found_word_idx].second.end(), current_word.second.begin(), current_word.second.end());
                    }

                }
            } else {
                // nu mai sunt liste nevizitate, se incrementeaza numarul de thread-uri care au finalizat agregarea
                tp->num_finished_aggregation++;
            }
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

    // citire numar fisiere
    string line;
    getline(fin, line);
    unsigned long files_num = stoul(line);
    cout << "Numarul de fisiere: " << files_num << "\n";

    // initializare threadpool
    threadpool_t tp;
    tp.num_mapper_threads = num_mapper_threads;
    tp.num_reducer_threads = num_reducer_threads;
    tp.num_finished_mapping = 0;
    tp.num_finished_aggregation = 0;
    tp.finished_reducing = false;
    tp.started_writing = false;
    pthread_mutex_init(&(tp.work_mutex), NULL);
    tp.threads = (pthread_t*) calloc(num_mapper_threads + num_reducer_threads, sizeof(pthread_t));
    tp.partial_lists = new partial_list_t[num_mapper_threads];
    tp.final_lists = new vector<pair<string, vector<unsigned int>>>[NUM_LETTERS];

    // initialize partial_lists bool
    for (size_t i = 0; i < tp.num_mapper_threads; i++) {
        tp.partial_lists[i].is_visited_by_reducer = false;
    }
    
    // adaugarea datelor fisierelor in coada
    for (size_t i = 1; i <= files_num; i++) {
        getline(fin, line);
        file_info_t file_info;
        file_info.file_id = i;
        file_info.file_path = line;
        tp.files_queue.push(file_info);
    }

    // se initializeaza thread-urile "mapper"
    for (unsigned int i = 0; i < num_mapper_threads; i++) {
        thread_struct_t *thread_struct = new thread_struct_t;
        thread_struct->thread_id = i;
        thread_struct->threadpool = &tp;
        int ret = pthread_create(&tp.threads[i], NULL, mapper_function, thread_struct);
        if (ret) {
            cout << "Eroare la crearea thread-ului " << i << "\n";
            exit(-1);
        }
    }

    // se initializeaza thread-urile "reducer"
    for (long int i = num_mapper_threads; i < num_mapper_threads + num_reducer_threads; i++) {
        int ret = pthread_create(&tp.threads[i], NULL, reducer_function, &tp);
        if (ret) {
            cout << "Eroare la crearea thread-ului " << i << "\n";
            exit(-1);
        }
    }

    cout << "Se asteapta toate thread-urile...\n";

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

    // se elibereaza memoria alocata pentru thread-uri si pentru liste
    free(tp.threads);
    delete[] tp.partial_lists;

    return 0;
}
