#include <iostream>
#include "toyexamples.hpp"
#include "graph.hpp"
#include "graphBuilder.hpp"
#include "../logging.hpp"

LOGGER("a")

int main() {
	INFO("Hello, A Bruijn!");
	CGraph graph = GraphBuilder().build();

	//ConstructDeBruijnGraph ( "ACTGACTGTTGACACTG", 9, 5 );
	return 0;
}
