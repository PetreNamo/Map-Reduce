#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <string.h>
#include <map>
#include <string>
#include <algorithm>

using namespace std;


struct MAP {
	int count, total;
	std::map<std::string, std::vector<int>> word_map;
	std::vector<string> files;
};

struct map_arg {
	int id;
	MAP *dictionary;
	pthread_mutex_t *mutex;
	pthread_barrier_t *barrier;
};

struct red_arg {
	int id;
	MAP *dictionary;
	pthread_mutex_t *mutex;
	pthread_barrier_t *barrier;
	std::vector<char> *letters;
};

bool customSort(const pair<string, vector<int>>& a, const pair<string, vector<int>>& b) {

	//functie pentru sortarea cuvintelor inainte de punerea in fisier
	//descrescator dupa numarul de aparitii si alfabetic in caz de egalitate
    if (a.second.size() != b.second.size())
    	return a.second.size() > b.second.size();
    return a.first < b.first;
}

void *f_map(void *arg)
{
	struct map_arg* data = (struct map_arg*) arg;

	while(data->dictionary->count < data->dictionary->total) {

		std::map<std::string, std::vector<int>> word_map_part;

		pthread_mutex_lock(data->mutex);
		if(data->dictionary->files.empty()==0 && data->dictionary->count < data->dictionary->total) {

			//fiecare thread ia un fisier din cele neparcurse pentru procesare
			std::string fisier = *(data->dictionary->files.begin());
			data->dictionary->files.erase(data->dictionary->files.begin());
			data->dictionary->count++;

			int nr = data->dictionary->count;

			std::ifstream file(fisier);
			string word;

			pthread_mutex_unlock(data->mutex);

			while(file>>word) {

				string newword;

				//se scot toare caracterele care nu sunt litere din cuvinte
				for(size_t i = 0; i<word.size(); i++)
					if(word[i] >= 'a' && word[i] <= 'z')
						newword += word[i];
					else if(word[i] >= 'A' && word[i] <= 'Z')
						newword += (word[i] + 32);

				word = newword;

				//se adauga cuvantul la lista partiala
				auto exists = word_map_part.find(word);
				if(exists == word_map_part.end())
					word_map_part[word] = {nr};
		
			}

			file.close();
			pthread_mutex_lock(data->mutex);

			//se adauga la lista generala
			for(auto& iterator : word_map_part)
				data->dictionary->word_map[iterator.first].insert(data->dictionary->word_map[iterator.first].end(), word_map_part[iterator.first].begin(), word_map_part[iterator.first].end());

			pthread_mutex_unlock(data->mutex);
			
		}
		else {
			pthread_mutex_unlock(data->mutex);
			break;
		}
			
	}

	pthread_barrier_wait(data->barrier);
	return NULL;
}

void *f_red(void *arg) {

	struct red_arg* data = (struct red_arg*) arg;

	pthread_barrier_wait(data->barrier);

	while(true) {

		pthread_mutex_lock(data->mutex);

		if(data->letters->empty() == 0){

			//se alege o litera din vector pentru a crea
			//fisierul respectiv de iesire
			char letter = *(data->letters->begin());
			data->letters->erase(data->letters->begin());

			pthread_mutex_unlock(data->mutex);

			vector<pair<string, vector<int>>> letter_map;

			for(auto& iterator : data->dictionary->word_map) {

				//punem in vector doar cuvintele care incep cu litera dorita
				if(iterator.first[0] == letter)
					letter_map.push_back({iterator.first, iterator.second});
			}

			//sortam indexii fisierelor
			for(auto& it : letter_map)
				sort(it.second.begin(), it.second.end());

			//sortam lista de cuvinte dupa criteriile dorite
			sort(letter_map.begin(), letter_map.end(), customSort);

			std::string fisier(1, letter);
			fisier += ".txt";

			std::ofstream output(fisier);

			for (const auto& pair : letter_map) {

        		output << pair.first << ":[";
				for(size_t i = 0; i < pair.second.size() - 1; i++)
            		output << pair.second[i] << " ";
				output << pair.second[pair.second.size()-1];
				output << "]"<< endl;

    		}

			output.close();
			
		}
		else {
			pthread_mutex_unlock(data->mutex);
			break;
		}

	}

	return NULL;

}

int main(int argc, char *argv[])
{
	
    int n;
	int r;

	int mapper = atoi(argv[1]);
	int reducer = atoi(argv[2]);
	char *file = argv[3];

	std::ifstream f(file);
    f >> n;

	void *status;
	pthread_t threads[mapper + reducer];
	struct map_arg arg_map[mapper];
	struct red_arg arg_red[reducer];

	pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

	pthread_barrier_t barrier;
	pthread_barrier_init(&barrier, NULL, mapper+reducer);

	struct MAP dictionary;
	dictionary.total = n;
	dictionary.count = 0;

	std::vector<char> letters;

	//cream "coada" cu litere pentru reduceri
	for(char i = 'a'; i <= 'z'; i++)
		letters.push_back(i);

	for(int i = 0; i < n; i++){

		//cream "coada" cu fisiere pentru mapperi
		std::string name;
		f >> name;
		dictionary.files.push_back(name);

	}

	for(int i = 0; i < mapper; i++){

		arg_map[i].id = i;
		arg_map[i].dictionary = &dictionary;
		arg_map[i].mutex = &mutex;
		arg_map[i].barrier = &barrier;

		r = pthread_create(&threads[i], NULL, f_map, &arg_map[i]);
		if (r)
			exit(-1);
	}

	for(int i = 0; i < reducer; i++) {
		arg_red[i].id = i + mapper;
		arg_red[i].dictionary = &dictionary;
		arg_red[i].mutex = &mutex;
		arg_red[i].letters = &letters;
		arg_red[i].barrier = &barrier;

		r = pthread_create(&threads[i+mapper], NULL, f_red, &arg_red[i]);
		if (r)
			exit(-1);
	}
	

    

	for (int i = 0; i < mapper + reducer; i++) {
		r = pthread_join(threads[i], &status);

		if (r)
			exit(-1);
	}

	pthread_mutex_destroy(&mutex);
	pthread_barrier_destroy(&barrier);
	f.close();

	return 0;
}
