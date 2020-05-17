#include <QtCore/QCoreApplication>
#include "mutexBenchMark.h"
#include "containerBenchMark.h"
#include "memoryMappingFileBenchmark.h"
#include "atomicBenchmark.h"
#include "vectorBenchmark.h"

using namespace std;
int main(int argc, char* argv[])
{
	QCoreApplication a(argc, argv);

	//mutexBenchMark::runBenchMark();
	//containerBenchMark::runBenchMark();
	//memoryMappingFileBenchmark::runBenchMark(); 
	//atomicBenchmark::runBenchMark();
	vectorBenchmark::runBenchMark();
	return 0;
}
