#ifndef TESTFRAMEWORK_H
#define TESTFRAMEWORK_H

#include "../graph.h"

#ifdef __cplusplus
extern "C" {
#endif

// numGraphs: the number of graphs that met the test criteria
//            (e.g. were generated and had a specific number of edges)
// numErrors: the number of graphs on which an algorithm or its integrity
//            check failed (i.e. produced NOTOK; should be 0)
// numOKs: the number of graphs on which the test produced OK
//         (as opposed to NONEMBEDDABLE or NOTOK)
// *_carry: The above are 32-bit integers, so we ensure that we can
//          take the result up to the capacity of a 64-bit integer
//          (and if a *_carry loops to 0, then we report an error)
typedef struct
{
	unsigned long numGraphs;
	unsigned long numErrors;
	unsigned long numOKs;
	unsigned long numGraphs_carry;
	unsigned long numErrors_carry;
	unsigned long numOKs_carry;
} baseTestResult;

// result: Cumulative result of tests on graphs of all sizes
//         (i.e. numbers of edges; the number of vertices is fixed
//          within a given test)
// edgeResults*: Array to accumulateresult of tests on graphs of a
//               each fixed size (i.e. fixed number of edges)
// theGraph: a graph data structure on which each test is performed
//           (the graph is preconfigured with a number of vertices
//            and a specific algorithm extension)
// origGraph: a copy of the graph being tested before the algorithm
//            is run on it. This is needed for integrity checking of
//            an algorithm's result produced in theGraph
typedef struct
{
	baseTestResult result;
	baseTestResult *edgeResults;
	int edgeResultsSize;
	graphP theGraph, origGraph;
} testResult;

typedef testResult * testResultP;

// algResults: An array of testResults for each algorithm being tested
// algResultsSize: the number of algorithms being tested
// algCommands: string mapping char commands for each algorithm to the
//              location of results for that algorithm in algResults
// testGraph: an adjacency list graph data structure into which each
//            adjacency matrix graph generated by Nauty is copied
//            (gp_CopyAdjacencyLists() is then used to copy the edges of
//             this graph into theGraph of each algorithm testResult).
typedef struct
{
	testResult *algResults;
	int algResultsSize;
	char *algCommands;
	graphP testGraph;
} testResultFramework;

typedef testResultFramework * testResultFrameworkP;

// Returns a pointer to the testResult for the given command character
// or NULL if there is no test result for the algorithm
testResultP getTestResult(char command);

// Allocate the test framework for the given command. The number of vertices
// and maximum number of edges in the graphs to be tested are also given.
testResultFrameworkP allocateTestFramework(char command, int n, int max_e);

// Free the test framework.
int freeTestFramework(testResultFrameworkP *pTestFramework);

#ifdef __cplusplus
}
#endif

#endif
