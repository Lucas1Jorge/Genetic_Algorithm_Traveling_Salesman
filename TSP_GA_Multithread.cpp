#include <bits/stdc++.h>

using namespace std;

const int num_threads = 4;
vector< vector< vector<double> > > next_generation[num_threads];
static mutex mtx;


// Read data from input file. Each line is expected in the format "city_number x_coordinate y_coordinate"
vector< vector<double> > getData() {
	vector< vector<double> > ans;

	int name;
	double x, y;
	while (scanf("%d %lf %lf", &name, &x, &y) != EOF) {
		vector<double> city(3);
		city[0] = name;
		city[1] = x;
		city[2] = y;

		ans.push_back(city);
	}

	return ans;
}


// print an individual path (all it's cities, in order)
void printIndividual(vector< vector<double> > A) {
	for (int i = 0; i < (int)A.size(); i++) {
		printf("%d ", (int)A[i][0]);
	}
	printf("\n");
}


// distance between 2 points in 2D plane (A[1] and A[2] are, respectively, the x and y coordinates)
double dist(vector<double> A, vector<double> B) {
	return sqrt(pow((A[1] - B[1]), 2) + pow((A[2] - B[2]), 2));
}


// Calculate the length of an individual path
double pathLen(vector< vector<double> > A) {
	double path = 0;
	for (int i = 0; i < A.size()-1; i++) {
		path += dist(A[i], A[i+1]);
	}
	return path;
}


// Comparison used to sort each population by the most adapted individuals (shortest paths)
struct compare {
	inline bool operator() (vector< vector<double> > A, vector< vector<double> > B) {
		double pathA = 0;
		double pathB = 0;

		for (int i = 0; i < A.size()-1; i++) {
			pathA += dist(A[i], A[i+1]);
		}

		for (int i = 0; i < B.size()-1; i++) {
			pathB += dist(B[i], B[i+1]);
		}

		return pathA < pathB;
	}
};


// Sort the cities in random order to generate a random individual
vector< vector<double> > createRandomIndividual(vector< vector<double> > data) {
	vector< vector<double> > ans;
	ans.push_back(data[0]);

	vector<int> index(data.size()-1);
	for (int i = 1; i < data.size(); i++)
		index[i-1] = i;

	while (index.size() > 0) {
		unsigned int seed = rand();
		int j = rand_r(&seed) % index.size();
		ans.push_back(data[index[j]]);

		index.erase(index.begin() + j);
	}

	ans.push_back(data[0]);
	return ans;
}


// Insert mutation in an individual (in a random position)
void mutate(vector< vector<double> >& A) {
	if (A.size() == 0)
		throw invalid_argument( "Mutate zero_leght individual" );

	unsigned int seed = rand();
	int erase = rand_r(&seed) % (A.size()-2) + 1;
	vector<double> temp = A[erase];
	A.erase(A.begin() + erase);

	int insert;
	do {
		seed = rand();
		insert = rand_r(&seed) % (A.size()-2) + 1;
	} while (insert == erase);
	A.insert(A.begin() + insert, temp);
}


// crossover between 2 parents producing 1 children
vector< vector<double> > crossover(vector< vector<double> > A, vector< vector<double> > B) {
	vector< vector<double> > ans(A.size());
	unsigned int seed = rand();
	if (rand_r(&seed) % 2 < 1) swap(A, B);

	set< vector<double> > contains;
	set<int> ocupied;

	ans[0] = A[0];
	ans[A.size()-1] = A[A.size()-1];
	contains.insert(A[A.size()-1]);
	ocupied.insert(A.size()-1);
	contains.insert(A[0]);
	ocupied.insert(0);

	seed = rand();
	int start = rand_r(&seed) % A.size();
	for (int i = 0; i <= A.size() / 2; i++) {
		int index = (start + i) % A.size();
		ans[index] = A[index];
		contains.insert(A[index]);
		ocupied.insert(index);
	}

	int j = 0;
	for (int i = 0; i < A.size(); i++) {
		if (ocupied.find(i) != ocupied.end())
			continue;
		
		while (j < B.size() && (contains.find(B[j]) != contains.end()))
			j++;

		if (j >= B.size()) break;

		contains.insert(B[j]);
		ocupied.insert(i);
		ans[i] = B[j];
	}
	for (int i = 0; i < A.size(); i++)
		if (ocupied.find(i) == ocupied.end())
			ans[i] = A[i];

	return ans;
}


// Mutual exclusive method to insert individuals in the critical region
void safe_insert(vector< vector<double> > individual, int next_gen_id, int population_size) {
	lock_guard<mutex> lock(mtx);
	if (next_generation[next_gen_id].size() >= population_size) return;
	next_generation[next_gen_id].push_back(individual);
}

// Genetic Algorithm to solve the Traveling Salesman Problem
void TSP_GA(vector< vector<double> > data, int population_size, int best_individuals, int number_of_children, int number_of_migrations, int number_of_generations, double mutation_probability, int t_id) {
	clock_t time = clock();
	vector< vector<double> > ans;

	// if (data.size() == 0) return;

	// Create initial population
	vector< vector< vector<double> > > population(population_size);
	for (int i = 0; i < population_size; i++)
		population[i] = createRandomIndividual(data);

	// Termination criteria: number of generations
	for (int generation = 0; generation <= number_of_generations; generation++) {

		// Sort population by individuals fitness (shortest paths prevail)
		sort(population.begin(), population.end(), compare());
		ans = population[0];

		// Migration: fitness based (best individuals migrate to other populations)
		for (int i = 0, j = 0; i < number_of_migrations; i++, j++) {
			if (j % num_threads == t_id) j++;
			safe_insert(population[i], i % num_threads, population_size);
		}
		
		// Crossover between best individuals
		for (int i = 0; i < best_individuals - 1; i++)
			for (int j = 0; j < number_of_children; j++)
				safe_insert(crossover(population[i], population[i+1]), t_id, population_size);

		// complete the next generation with some random (lucky) individuals from previous generation
		set<int> picked;
		while (next_generation[t_id].size() < population_size) {
			int lucky;

			do {
				unsigned int seed = rand();
				lucky = rand_r(&seed) % population_size;
			} while (picked.find(lucky) != picked.end());

			picked.insert(lucky);
			safe_insert(population[lucky], t_id, population_size);
		}

		// Mutation
		for (int i = 0; i < next_generation[t_id].size(); i++) {
			unsigned int seed = rand();
			if (rand_r(&seed) % 100 < mutation_probability * 100)
				mutate(next_generation[t_id][i]);
		}

		// update current generation
		for (int i = 0; i < population_size; i++)
			population[i] = next_generation[t_id][i];
		next_generation[t_id].clear();

	}

	// print current population(thread) length, elapsed_time and path
	time = clock() - time;
	printf("%d 		|	%lf	|	%lf	|	", t_id, pathLen(ans), double(time)/CLOCKS_PER_SEC);
	printIndividual(ans);
}



int main() {
	vector< vector<double> > data = getData();

	int population_size = 100;
	int best_individuals = 20;
	int number_of_children = 4;
	int number_of_migrations = 4;
	int number_of_generations = 30;
	double mutation_probability = 0.1;

	// Print the results
	printf("\nThread_id	|	Lenght 		|	Elapsed_time	|	Path\n");

	thread t[num_threads];
	for (int i = 0; i < num_threads; i++) {
		t[i] = thread(TSP_GA, data, population_size, best_individuals, number_of_children, number_of_migrations, number_of_generations, mutation_probability, i);
	}

	for (int i = 0; i < num_threads; i++) {
		t[i].join();
	}

	return 0;
}