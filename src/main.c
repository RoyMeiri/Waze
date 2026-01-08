#include <stdio.h>
#include <stdlib.h>

#include "graph.h"
#include "graph_loader.h"

int main(void) {
    printf("MAIN: started\n");
    fflush(stdout);

    const char* meta  = "data/graph.meta";
    const char* nodes = "data/nodes.csv";
    const char* edges = "data/edges.csv";

    /* Allocate on heap to avoid stack overflow (Graph holds MAX_NODES array) */
    Graph* g = (Graph*)malloc(sizeof(Graph));
    if (!g) {
        fprintf(stderr, "MAIN: failed to allocate graph\n");
        return 1;
    }

    printf("MAIN: about to load graph...\n");
    fflush(stdout);

    int rc = graph_load_from_files(g, meta, nodes, edges);
    if (rc != 0) {
        fprintf(stderr, "MAIN: Failed to load graph (rc=%d)\n", rc);
        free(g);
        return 1;
    }

    printf("Loaded graph: num_nodes=%d num_edges=%d\n", g->num_nodes, g->num_edges);

    printf("Outgoing edges from node 0:\n");
    EdgeNode* cur = g->nodes[0].out_edges;
    while (cur) {
        int eid = cur->edge_id;
        Edge* e = &g->edges[eid];
        printf("  edge %d: %d -> %d, time=%lf\n",
               eid, e->from_node, e->to_node, e->current_travel_time);
        cur = cur->next;
    }

    graph_free(g);
    free(g);
    printf("MAIN: done\n");
    return 0;
}
