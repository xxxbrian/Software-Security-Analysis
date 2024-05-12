#include "GraphTraversal.h"
#include "cassert"

void Test1() {
    /*


        1
       /  \
      2   3
       \ /
        4
        |
        5

  */
    // init nodes
    Node *node1 = new Node(1);
    Node *node2 = new Node(2);
    Node *node3 = new Node(3);
    Node *node4 = new Node(4);
    Node *node5 = new Node(5);

    // init edges
    Edge *edge1 = new Edge(node1, node2);
    Edge *edge2 = new Edge(node1, node3);
    node1->addOutEdge(edge1);
    node1->addOutEdge(edge2);
    Edge *edge3 = new Edge(node2, node4);
    Edge *edge4 = new Edge(node3, node4);
    node2->addOutEdge(edge3);
    node3->addOutEdge(edge4);
    Edge *edge5 = new Edge(node4, node5);
    node4->addOutEdge(edge5);

    // init Graph
    Graph *g = new Graph();
    g->addNode(node1);
    g->addNode(node2);
    g->addNode(node3);
    g->addNode(node4);
    g->addNode(node5);
    // test
    std::set<std::string> expected_answer{"START: 1->2->4->5->END", "START: 1->3->4->5->END"};
    GraphTraversal *dfs = new GraphTraversal();
    dfs->DFS(node1, node5);
    assert(dfs->getPaths() == expected_answer && "Test case 1 failed!");
    std::cout << "Test case 1 passed!\n";
}

/// Entry of the program
int main() {
    Test1();
    return 0;
}