#pragma once
#include <vector>

namespace vectorBenchmark {
	using namespace std;
	template <typename T, class _Alloc = allocator<T>>
	class bivector
	{
		using size_type = unsigned int;
		using _Vec_Alloc = allocator<vector<T, _Alloc>>;
		using _Vec_Declare = vector<T, _Alloc>;
	public:
		bivector(int innerCap = 5000) {
			nSize = 0;
			nInnerCap = innerCap;
		}
		~bivector() {}
		void push_back(const T& val) {
			size_type m = nSize / nInnerCap;
			if (m >= innerVector.size()) {
				_Vec_Declare vec(0);
				vec.reserve(nInnerCap);
				innerVector.emplace_back(vec);
			}
			innerVector[m].push_back(val);
			nSize++;
		}
		T& operator[](const size_type pos) {
			size_type m = nSize / nInnerCap, n = pos % nInnerCap;
			return innerVector[m][n];
		}
		void clear() {
			for (auto& it : innerVector) {
				it.clear();
			}
			innerVector.clear();
			nSize = 0;
		}

	private:
		vector<vector<T, _Alloc>, _Vec_Alloc> innerVector;
		int nInnerCap;///innerVector 中单个vector的容量大小
		size_type nSize;
	};

	bivector<int> biVector;
	vector<int> siVector;
	auto maxSize = (unsigned int)1 << 20;

	int runBenchMark() {
		cout << "vector size:" << maxSize << endl;
		for (decltype(maxSize) i = 0; i < maxSize; i++) {
			siVector.push_back(i);
		}
		siVector.clear();
		for (decltype(maxSize) i = 0; i < maxSize; i++) {
			biVector.push_back(i);
		}
		biVector.clear();
		return 0;
	}
}