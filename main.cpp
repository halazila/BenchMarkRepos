#include <QtCore/QCoreApplication>
#include "mutexBenchMark.h"
#include "containerBenchMark.h"
#include "VectorMMF.h"

using namespace std;
int main(int argc, char* argv[])
{
	QCoreApplication a(argc, argv);

	mutexBenchMark::runBenchMark();
	//containerBenchMark::runBenchMark();
	return 0;
}
