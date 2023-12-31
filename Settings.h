#pragma once
#include <boost/graph/adjacency_list.hpp>

// Default settings and some custom type definitions
typedef boost::adjacency_list<
    boost::vecS,                                   // Vertex storage
    boost::vecS,                                   // OutEdge storage
    boost::undirectedS,                              // Directed or undirected
    boost::no_property,                            // No vertex property
    boost::property<boost::edge_weight_t, double>  // Edge property with weight
> LocationUndirectedGraph;

static const bool SAVE_CSV = true;
static const bool SAVE_GRAPHVIZ = true;
static const bool SHOW_EPIDEMIC_RESULTS = true;

static const int DEFAULT_NUMBER_OF_THREADS = 4;

static const std::uint8_t DEFAULT_TOTAL_EPOCHS = 30;
static const int DEFAULT_INDIVIDUAL_COUNT = 1000;

static const int INITIAL_INFECTED_COUNT = 15;

static const std::uint8_t DEFAULT_REPEAT_COUNT = 4;

static const int CHUNK_SIZE_DIVIDER = 10;
