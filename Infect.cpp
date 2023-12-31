#include <omp.h>
#include <ctime>
#include <iostream>
#include <sstream>
#include "Individual.h"
#include "GraphHandler.h"
#include "Settings.h"
#include <fstream>
#include <numeric>


using namespace boost;
using namespace std;

vector<int>* population = nullptr;

//write function
void write_vector(vector<vector<int>>& myVector,  string& filename) {
    ofstream outputFile(filename);

    // write the element to the file
    for (auto& row : myVector) {
        for (int& element : row) {
            outputFile << element << ",";
        }
        outputFile << endl;
    }
    outputFile.close();
}

void simulate_parallel(int individual_count, int location_count, int total_epochs, const LocationUndirectedGraph& individual_graph,
	vector<Individual>& individuals) {

	int index = 0;
	int max_index = int(individuals.size());
	int chunk = int(max_index / DEFAULT_NUMBER_OF_THREADS);
///////////################################################################
	vector<vector<int>> epoch_population;	
	vector<std::tuple<int, int, int>> epoch_statistics;
///////////################################################################
	// Generate a look up map with the neighbouring nodes for each graph node
	boost::unordered_map<int, vector<int>> neighborhood_lookup_map = GraphHandler::get_node_neighborhood_lookup_map(individual_graph);

	// Repeat for all the epochs
	for (int epoch = 0; epoch < total_epochs; epoch++) {
///////////################################################################

		population = new vector<int>(location_count, 0);
		omp_lock_t writelock;
		omp_init_lock(&writelock);

		//	Calculate the population at each location
		#pragma omp parallel private(index) shared(individuals) firstprivate(chunk, max_index)
		{
			#pragma omp for schedule(static, chunk) nowait
			
			for (index = 0; index < max_index; ++index) {
				Individual current_individual = individuals[index]; // Thread local variable
				
				int current_location = current_individual.get_location(); // Thread local variable

				omp_set_lock(&writelock);

				(*population)[current_location] += 1;
				
				omp_unset_lock(&writelock);

				individuals[index] = current_individual; // Save individual back to the shared memory space

			}
			
		} // Implicit Barrier
		//int sum = accumulate(population->begin(), population->end(), 0);
    	// Print the sum
   // 	cout << "total population: " << sum << endl;

///////////################################################################	
		
		//	Randomly move all individuals
		auto weight_map = boost::get(boost::edge_weight, individual_graph);
		#pragma omp parallel private(index) shared(individuals, neighborhood_lookup_map) firstprivate(chunk, max_index)
		{
			#pragma omp for schedule(static, chunk) nowait
			for (index = 0; index < max_index; index++) {

				Individual current_individual = individuals[index]; // Thread local variable
				int current_location = current_individual.get_location(); // Thread local variable
				/////////////#################
				vector<int> node_neighbours = neighborhood_lookup_map[current_location]; // Thread local variable, get the location's neighbourhood
				
				map<int,int> weights;
				boost::graph_traits<LocationUndirectedGraph>::out_edge_iterator ei, ei_end;
				for (tie(ei, ei_end) = out_edges(current_individual.get_location(), individual_graph); ei != ei_end; ei++) {
					//cout << "Edge: " << *ei << ", Weight: " << weight_map[*ei] << endl;
					auto target_vertex = boost::target(*ei, individual_graph);
					weights[target_vertex]= weight_map[*ei];

				}
				current_individual.move(node_neighbours,weights,population,individual_count,location_count); // Stay in the same spot or move to a neighbouring node
                /////////////#################
				individuals[index] = current_individual; // Save individual back to the shared memory space
			}
		} // Implicit Barrier
		
///////////################################################################		
		// Try to infect individuals that are close to infected ones
		#pragma omp parallel private(index) shared(individuals) firstprivate(chunk, max_index)
		{
			// Since we only change individuals that are "chunked" by index for each thread, there is no need for critical/atomic region
			#pragma omp for schedule(auto) nowait
			for (index = 0; index < max_index; index++) {
				if (!individuals[index].is_infected()) { // Don't copy the shared memory element, just check a boolean
					Individual current_individual = individuals[index]; // Thread local variable
					int affecting_index;
					for (affecting_index = 0; affecting_index < individual_count; affecting_index++) {

						if (individuals[affecting_index].is_infected()) { // First do the binary check, then do the comparison because it is faster
							Individual affecting_individual = individuals[affecting_index]; // Thread local variable
							if (current_individual.get_location() == affecting_individual.get_location()) { // Now do the "expensive" comparison
								current_individual.try_infect();
								if (current_individual.is_infected()) { // Don't save to shared memory if the invidual wasn't eventually infected
									individuals[index] = current_individual; // Save affecting individual back to the shared memory space
									break; // No need to find other infected individuals in the same location, move the the next one
								}
							}
						}
					}
				}
			}

		} // Implicit Barrier

		// Advance the epoch for every individual and gather infected & hit statistics
		int hit_count = 0;
		int infected_count = 0;
		int recovered_count = 0;
		#pragma omp parallel private(index) shared(individuals) firstprivate(chunk, max_index) reduction(+:infected_count, hit_count, recovered_count)
		{
			// Since we only change individuals that are "chunked" by index for each thread, there is no need for critical/atomic region
			#pragma omp for schedule(static, chunk) nowait
			for (index = 0; index < max_index; index++) {		
				Individual current_individual = individuals[index]; // Thread local variable
				current_individual.advance_epoch();	// Check individuals for the number of epochs they're infected and tag them as healed and recovered if a threshold disease_duration is passed					
				individuals[index] = current_individual; // Save individual back to the shared memory space
				// Near the end of the parallel region, perform reduction. Gather statistics about the current advance_epoch : what is the fraction of infected and hit individual
				if (current_individual.is_infected())
					infected_count++;
				if (current_individual.is_hit())
					hit_count++;
				if (current_individual.is_recovered())
					recovered_count++;
			}
		} // Implicit Barrier

		epoch_statistics.push_back(make_tuple(hit_count, infected_count, recovered_count)); // Store tuple of statistics for the current epoch


///////////################################################################	
		epoch_population.push_back((*population));

	}
	

	string filename = "output_population_parallel.txt";
	write_vector(epoch_population, filename);

///////////################################################################		
	if (SAVE_CSV)
		GraphHandler::save_epoch_statistics_to_csv("output_statistics_parallel.csv", epoch_statistics);
	if (SAVE_GRAPHVIZ)
		GraphHandler::save_undirected_graph_to_graphviz_file("individualGraph.dot", individual_graph);
	if (SHOW_EPIDEMIC_RESULTS)
		GraphHandler::show_epidemic_results(individual_count, epoch_statistics);
}

void simulate_serial_naive(int individual_count, int location_count, int total_epochs, const LocationUndirectedGraph& individual_graph, vector<Individual>& individuals) {
	
	// Statistics vector, index is epoch
	vector<std::tuple<int, int, int>> epoch_statistics;
	
	// Generate a look up map with the neighbouring nodes for each graph node
	boost::unordered_map<int, vector<int>> neighborhood_lookup_map = GraphHandler::get_node_neighborhood_lookup_map(individual_graph);

	/*
	for(auto a:neighborhood_lookup_map){

		int current=a.first;

		vector<int> v=a.second;

		std::cout<<"current: " <<current<<" = ";
		for(auto b:v){

			std::cout<<b<<" ";
		}
		std::cout<<std::endl;

	}*/
///////////################################################################
	vector<vector<int>> epoch_population;	
///////////################################################################	

	// Repeat for all the epochs
	for (int current_epoch = 0; current_epoch < (total_epochs + 1); ++current_epoch) {

///////////################################################################

		population = new vector<int>(location_count, 0);
		
		
			
		for (Individual& current_individual : individuals){
			int current_location = current_individual.get_location(); // Thread local variable
			(*population)[current_location] += 1;
		}
		//int sum = std::accumulate(population->begin(), population->end(), 0);
    	// Print the sum
    //	std::cout << "total population: " << sum << std::endl;

///////////################################################################	



///////////################################################################
		//	Randomly move all individuals
		auto weight_map = boost::get(boost::edge_weight, individual_graph);
		
		for (Individual& current_individual : individuals){
			std::vector<int> node_neighbours = neighborhood_lookup_map[current_individual.get_location()];

			
			map<int,int> weights;
			boost::graph_traits<LocationUndirectedGraph>::out_edge_iterator ei, ei_end;
    		for (tie(ei, ei_end) = out_edges(current_individual.get_location(), individual_graph); ei != ei_end; ++ei) {
        		//std::cout << "Edge: " << *ei << ", Weight: " << weight_map[*ei] << std::endl;
				auto target_vertex = boost::target(*ei, individual_graph);
				weights[target_vertex]= weight_map[*ei];

    		}
			current_individual.move(node_neighbours,weights,population,individual_count,location_count); // Stay in the same spot or move to a neighbouring node
		}
///////////################################################################		
		// foreach each individual		
		for (int individual_index = 0; individual_index != individuals.size(); ++individual_index) {			
			
			if (individuals[individual_index].is_infected()) { // if the individual is infected

				// and meets another individual that is susceptible the disease
				for (int affecting_individual = 0; affecting_individual != individuals.size(); ++affecting_individual) {
					if (individual_index != affecting_individual) {

						// Check if the susceptible individual gets infected
						if (individuals[individual_index].get_location() == individuals[affecting_individual].get_location()) // in the same location
							individuals[affecting_individual].try_infect();
					}
				}
			}
		}
		
		int hit_count = 0;
		int infected_count = 0;
		int recovered_count = 0;
		for (Individual& current_individual : individuals) {
			// Check individuals for the number of epochs they're infected and tag them as healed and recovered if a threshold disease_duration is passed
			current_individual.advance_epoch();

			// Gather statistics about the current advance_epoch : what is the fraction of infected and hit individual
			if (current_individual.is_infected())
				++infected_count;
			if (current_individual.is_hit())
				++hit_count;
			if (current_individual.is_recovered())
				++recovered_count;
		}
		epoch_statistics.push_back(std::make_tuple(hit_count, infected_count, recovered_count));
///////////################################################################	
		epoch_population.push_back((*population));

	}
	string filename = "output_population_serial.txt";
	write_vector(epoch_population, filename);

///////////################################################################	
	
	if (SAVE_CSV)
		GraphHandler::save_epoch_statistics_to_csv("output_statistics_serial.csv", epoch_statistics);
	if (SAVE_GRAPHVIZ)
		GraphHandler::save_undirected_graph_to_graphviz_file("individualGraph.dot", individual_graph);
	if (SHOW_EPIDEMIC_RESULTS)
		GraphHandler::show_epidemic_results(individual_count, epoch_statistics);
}


void reset_input(string filename, int individual_count, int& location_count, int& edge_count, LocationUndirectedGraph& individual_graph, vector<Individual>& individuals) {
	
	//load graph nodes and edges from filename
	individual_graph = GraphHandler::get_location_undirected_graph_from_file(filename); 

	//get graph information
	location_count = boost::num_vertices(individual_graph);
	edge_count = boost::num_edges(individual_graph);

	//generate random positions for each individual
	individuals = GraphHandler::get_random_individuals(individual_count, location_count); 
    /*for(auto ind:individuals){
        cout<<ind.get_location()<<" ";
    }*/

	//randomly infect some individuals
	for (int i = 0; i < INITIAL_INFECTED_COUNT; ++i) {
		individuals[i].infect();
	}
}


int main() {

	

		// Experiment parameters
		int max_thread = 4; //DEFAULT_NUMBER_OF_THREADS;
		int individual_count = 1000; //DEFAULT_INDIVIDUAL_COUNT;
		int total_epochs = 30; //DEFAULT_TOTAL_EPOCHS;
		string input_graph_filename = "myedge.edges";//"minimumantwerp.edges"; // Read locations from the full Antwerp graph or from a minimal version (500 nodes)

	

		// Cout experiments parameters
		cout << "----- Dragonfly model simulation -----" << endl;
		cout << "Max number of threads: " << max_thread << endl;
		cout << "Number of individual: " << individual_count << endl;
		cout << "Total Epochs: " << total_epochs << endl;
		cout << "Graph from file: " << input_graph_filename << endl;
		

		//Initialization
		LocationUndirectedGraph individual_graph; 
		int location_count, edge_count;
		vector<Individual> individuals; 
		

		// Reset individuals
		reset_input(input_graph_filename, individual_count, location_count, edge_count, individual_graph, individuals);
		cout << "Number of nodes: " << location_count << endl; 
		cout << "Number of edges: " << edge_count << endl; 

		//Record time
		double time_start;

///////////################################################################	
		// Serial
		cout << "Serial version...";
		
		time_start = omp_get_wtime();
		simulate_serial_naive(individual_count, location_count, total_epochs, individual_graph, individuals);
		cout << "Serial time = " << (omp_get_wtime() - time_start) * 1000.0 << " ms" << endl;

///////////################################################################	

		// Reset individuals
		reset_input(input_graph_filename, individual_count, location_count, edge_count, individual_graph, individuals);

		// Parallel using OpenMP
		for(int thread = max_thread; thread >= 1;){

			omp_set_num_threads(thread);

			cout << endl << "Running with OpenMP using "<< thread <<" threads ...";
				
				
			time_start = omp_get_wtime();
			simulate_parallel(individual_count, location_count, total_epochs, individual_graph, individuals);

			// Reset individuals
			reset_input(input_graph_filename, individual_count, location_count, edge_count, individual_graph, individuals); 

			cout << "Parallel time with "<<thread<<" threads = "<< (omp_get_wtime() - time_start) * 1000.0 << " ms" << endl;
			thread = thread /2;
		}
		
	
}
