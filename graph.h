#ifndef GRAPH_H
#define GRAPH_H

#include <stdlib.h>

#define MAX_NODES 100000 // Define a maximum number of nodes for simplicity

typedef struct {
    int edge_id;
    int to_node;              // Destination node ID
    double base_length;       // Physical length
    double current_travel_time; // Weight for calculation
} Edge;

// Node structure in adjacency list
// Used to build the linked list within the graph
typedef struct EdgeNode {
    Edge edge_data;
    struct EdgeNode* next;
} EdgeNode;

// Node structure in graph
typedef struct {
    int node_id;
    double x, y;          // For A* heuristic
    EdgeNode* out_edges;  // Head of the linked list of outgoing edges
} Node;

// Graph structure
typedef struct {
    Node nodes[MAX_NODES];
    int num_nodes;
} Graph;

#endif