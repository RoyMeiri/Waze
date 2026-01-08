#include <stdio.h>
#include <stdlib.h>
#include "graph.h"
#include "routing.h"

void test_same_start_target(void);
void test_disconnected_graph(void);
void test_directed_edge(void);
void test_multiple_paths(void);
void test_heuristic(void);

/* Helper for visual separation */
static void separator(const char* title) {
    printf("\n==============================\n");
    printf("%s\n", title);
    printf("==============================\n");
}

/* Allocate graph on the heap to avoid stack overflow (Graph has MAX_NODES). */
static Graph* create_graph(int num_nodes, int num_edges) {
    Graph* g = (Graph*)malloc(sizeof(Graph));
    if (!g) {
        fprintf(stderr, "Failed to allocate Graph\n");
        exit(1);
    }
    graph_init(g, num_nodes, num_edges);
    return g;
}

static void destroy_graph(Graph* g) {
    if (!g) return;
    graph_free(g);
    free(g);
}

/* TEST 1: start == target */
void test_same_start_target() {
    separator("TEST 1: start == target");

    Graph* g = create_graph(1, 0);
    graph_set_node_coordinates(g, 0, 0.0, 0.0);

    printf("Calling A* (0 -> 0)\n");
    find_route_a_star(g, 0, 0);
    printf("Returned from A*\n");

    destroy_graph(g);
}

/* TEST 2: disconnected graph */
void test_disconnected_graph() {
    separator("TEST 2: disconnected graph");

    Graph* g = create_graph(2, 0);

    graph_set_node_coordinates(g, 0, 0.0, 0.0);
    graph_set_node_coordinates(g, 1, 10.0, 10.0);

    printf("Calling A* (0 -> 1)\n");
    find_route_a_star(g, 0, 1);
    printf("Returned from A*\n");

    destroy_graph(g);
}

/* TEST 3: directed edge behavior */
void test_directed_edge() {
    separator("TEST 3: directed edge");

    Graph* g = create_graph(2, 1);

    graph_set_node_coordinates(g, 0, 0.0, 0.0);
    graph_set_node_coordinates(g, 1, 1.0, 0.0);

    /* Only 0 -> 1 exists */
    graph_add_edge(g, 0, 0, 1, 10.0, 10.0);

    printf("Calling A* (1 -> 0) [should fail]\n");
    find_route_a_star(g, 1, 0);
    printf("Returned from A*\n");

    destroy_graph(g);
}

/* TEST 4: multiple paths, choose cheapest */
void test_multiple_paths() {
    separator("TEST 4: multiple paths (choose cheapest)");

    Graph* g = create_graph(3, 3);

    graph_set_node_coordinates(g, 0, 0.0, 0.0);
    graph_set_node_coordinates(g, 1, 1.0, 0.0);
    graph_set_node_coordinates(g, 2, 2.0, 0.0);

    /* Expensive direct path: 0 -> 2 */
    graph_add_edge(g, 0, 0, 2, 100.0, 10.0); // cost = 10

    /* Cheap path: 0 -> 1 -> 2 */
    graph_add_edge(g, 1, 0, 1, 10.0, 10.0);  // cost = 1
    graph_add_edge(g, 2, 1, 2, 10.0, 10.0);  // cost = 1

    printf("Calling A* (0 -> 2)\n");
    find_route_a_star(g, 0, 2);
    printf("Returned from A*\n");

    destroy_graph(g);
}

/* TEST 5: heuristic correctness */
void test_heuristic() {
    separator("TEST 5: heuristic correctness");

    Graph* g = create_graph(2, 0);

    graph_set_node_coordinates(g, 0, 3.0, 4.0);
    graph_set_node_coordinates(g, 1, 0.0, 0.0);

    double h = heuristic(g, 0, 1);
    printf("Heuristic distance = %.2f (expected 5.00)\n", h);

    destroy_graph(g);
}

int main() {
    printf("=== ENTERED TEST_ALL MAIN ===\n");

    test_same_start_target();
    test_disconnected_graph();
    test_directed_edge();
    test_multiple_paths();
    test_heuristic();

    printf("\nAll tests completed.\n");
    return 0;
}
