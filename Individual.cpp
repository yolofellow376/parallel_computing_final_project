#include <random>
#include <iostream>
#include <map>
#include<cmath>
#include "Individual.h"
using namespace std;
// Check if an individual gets infected by a predefined chance
void Individual::try_infect() {

	if (!infected_) {
		if (get_random_infect_chance() < parameters_.Infectiosity)
			infect();
	}
}

float Individual::get_random_infect_chance() {

	std::random_device random_device;
	std::mt19937 mersenne_twister_engine(random_device());
	std::uniform_real_distribution<> real_random(0, 1);

	return static_cast<float>(real_random(mersenne_twister_engine));
}
//////////////////////////##################################################
int Individual::get_random_location(map<int,int> weights,std::vector<int>& node_neighbours, std::vector<int>* population, int individual_count, int location_count) {
	std::random_device random_device;
	std::mt19937 mersenne_twister_engine(random_device());

	// Create a discrete distribution based on edge weights
    std::vector<double> probabilities;

	//std::cout<<"chk1"<<std::endl;
// mod by Alan ============================================================ 
    for (int neighbor : node_neighbours) {
		//td::cout<<"chk2:　"<<neighbor<<std::endl;
		int n = (*population)[neighbor]; //+2 to avoid problem when population == 0 or 1
		//std::cout<<"chk3:　"<<neighbor<<std::endl;
		int Total = individual_count;
		int Locations = location_count;
		//std::cout<<"chk4:　"<<neighbor<<std::endl;
		double weight1 = exp( -pow(((n-Total/sqrt(Locations))/(Total/sqrt(Locations)/3)),2) );
		double weight2 = exp( -pow((n/(Total/sqrt(Locations)/10)),2) );
		//std::cout<<"chk5:　"<<neighbor<<std::endl;
		weights[neighbor] = weight1+weight2;
		//std::cout<<"chk6:　"<<neighbor<<std::endl;
		//cout<<"neighbor: "<<neighbor<<"weights.at(neighbor): "<<weights[neighbor]<<endl;
        probabilities.push_back(weights[neighbor]);
    }
// end mod by Alan ========================================================

	// Normalize probabilities to make sure they sum to 1
    double total_weight = std::accumulate(probabilities.begin(), probabilities.end(), 0.0);
    for (double& prob : probabilities) {
        prob /= total_weight;
    }

	//cout<<"probabilites.size(): "<<probabilities.size()<<endl;
	//std::discrete_distribution<> discrete_distribution(probabilities.begin(), probabilities.end());
	//return discrete_distribution(mersenne_twister_engine);

	std::uniform_int_distribution<> uniform_int_distribution(0, static_cast<int>(node_neighbours.size())-1); // Since we added the current location, no need to decarase the max int

	return uniform_int_distribution(mersenne_twister_engine);
}

// Randomly move the individual to another location or stay at the same location
void Individual::move(std::vector<int>& node_neighbours, map<int,int> weights, std::vector<int>* population, int individual_count, int location_count) {
    
	//std::cout<<"chk0"<<std::endl;
	node_neighbours.push_back(location_); // Add current location in the new locations vector
	weights[location_]=100;
	location_ = node_neighbours[get_random_location( weights, node_neighbours, population, individual_count, location_count)]; // Assign the random location
}
//////////////////////////##################################################
