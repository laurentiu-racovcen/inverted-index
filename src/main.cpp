#include <iostream>
#include <fstream>
#include <pthread.h>

using namespace std;

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
    ifstream input_file(argv[3]);
    if (!input_file) {
        cerr << "\nNu a putut fi deschis fisierul!\n\n";
        exit(3);
    }

    cout << "Numarul de mapper threads: " << num_mapper_threads << "\n";
    cout << "Numarul de reducer threads: " << num_reducer_threads << "\n";
    cout << "Fisierul citit: " << argv[3] << "\n";

    // pthread_t threads[];

    return 0;
}
