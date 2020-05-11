#pragma once
#include <QVector>
#include <chrono>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <qhash.h>
#include <qmap.h>
#include <map>

namespace containerBenchMark {
	using namespace std;
	///generate random str
	void gen_random(char* s, const int len) {
		static const char alphanum[] =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";

		for (int i = 0; i < len; ++i) {
			s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
		}
		s[len] = 0;
	}

	void funcHashMap() {
		cout << "********hash && map insert benchmark**********" << endl;
		static int repeatCount = 1000000;
		char str[101];
		int len;
		QHash<QByteArray, int> qHashMap;
		QMap<QByteArray, int> qMap;
		unordered_map<string, int> unorderedMap;
		map<string, int> orderedMap;

		chrono::steady_clock::time_point startTime;
		chrono::steady_clock::time_point endTime;

		cout << "insert " << repeatCount << " pairs\n\r";

		///QHash 
		startTime = chrono::steady_clock::now();
		for (int i = 0; i < repeatCount; i++) {
			len = rand() % 100;
			gen_random(str, len);
			qHashMap[QByteArray(str)] = len;
		}
		endTime = chrono::steady_clock::now();
		cout << "QHash<QByteArray, int> insert :          " << chrono::duration_cast<chrono::microseconds>(endTime - startTime).count() << " us \n\r";

		///QMap
		startTime = chrono::steady_clock::now();
		for (int i = 0; i < repeatCount; i++) {
			len = rand() % 100;
			gen_random(str, len);
			qMap[QByteArray(str)] = len;
		}
		endTime = chrono::steady_clock::now();
		cout << "QMap<QByteArray, int> insert :           " << chrono::duration_cast<chrono::microseconds>(endTime - startTime).count() << " us \n\r";

		///std unordered_map
		startTime = chrono::steady_clock::now();
		for (int i = 0; i < repeatCount; i++) {
			len = rand() % 100;
			gen_random(str, len);
			unorderedMap[string(str)] = len;
		}
		endTime = chrono::steady_clock::now();
		cout << "std::unordered_map<string, int> insert : " << chrono::duration_cast<chrono::microseconds>(endTime - startTime).count() << " us \n\r";

		///std ordered_map
		startTime = chrono::steady_clock::now();
		for (int i = 0; i < repeatCount; i++) {
			len = rand() % 100;
			gen_random(str, len);
			orderedMap[string(str)] = len;
		}
		endTime = chrono::steady_clock::now();
		cout << "std::map<string, int> insert :           " << chrono::duration_cast<chrono::microseconds>(endTime - startTime).count() << " us \n\r";

		cout << "********hash && map search benchmark**********" << endl;
		cout << "search " << repeatCount << " times\n\r";

		///QHash
		startTime = chrono::steady_clock::now();
		for (int i = 0; i < repeatCount; i++) {
			len = rand() % 100;
			gen_random(str, len);
			qHashMap.find(QByteArray(str));
		}
		endTime = chrono::steady_clock::now();
		cout << "QHash<QByteArray, int> search :          " << chrono::duration_cast<chrono::microseconds>(endTime - startTime).count() << " us \n\r";

		///QMap
		startTime = chrono::steady_clock::now();
		for (int i = 0; i < repeatCount; i++) {
			len = rand() % 100;
			gen_random(str, len);
			qMap.find(QByteArray(str));
		}
		endTime = chrono::steady_clock::now();
		cout << "QMap<QByteArray, int> search :           " << chrono::duration_cast<chrono::microseconds>(endTime - startTime).count() << " us \n\r";

		///std unordered_map
		startTime = chrono::steady_clock::now();
		for (int i = 0; i < repeatCount; i++) {
			len = rand() % 100;
			gen_random(str, len);
			unorderedMap.find(string(str));
		}
		endTime = chrono::steady_clock::now();
		cout << "std::unordered_map<string, int> search : " << chrono::duration_cast<chrono::microseconds>(endTime - startTime).count() << " us \n\r";

		///std ordered_map
		startTime = chrono::steady_clock::now();
		for (int i = 0; i < repeatCount; i++) {
			len = rand() % 100;
			gen_random(str, len);
			orderedMap.find(string(str));
		}
		endTime = chrono::steady_clock::now();
		cout << "std::map<string, int> search :           " << chrono::duration_cast<chrono::microseconds>(endTime - startTime).count() << " us \n\r";

	}

	void funcVectorCopy() {
		cout << "********vector copy benchmark**********" << endl;
		QVector<int> qvec1, qvec2(100000000, 3), qvec3;
		qvec1.fill(1, 100000000);
		qvec2.clear();
		auto start = chrono::steady_clock::now();
		qvec2 << qvec1;
		auto end = chrono::steady_clock::now();
		auto duration = chrono::duration_cast<chrono::microseconds>(end - start);
		cout << "QVector operator<<:        " << double(duration.count()) << " us" << endl;

		start = chrono::steady_clock::now();
		qvec3 = qvec1;
		end = chrono::steady_clock::now();
		duration = chrono::duration_cast<chrono::microseconds>(end - start);
		cout << "QVector operator=:         " << double(duration.count()) << " us" << endl;

		start = chrono::steady_clock::now();
		qvec1[0] = 2;
		end = chrono::steady_clock::now();
		duration = chrono::duration_cast<chrono::microseconds>(end - start);
		cout << "QVector copy when write:   " << double(duration.count()) << " us" << endl;

		vector<int> vec1, vec2, vec3;
		vec1.resize(100000000);
		fill(vec1.begin(), vec1.end(), 1);
		start = chrono::steady_clock::now();
		vec2 = vec1;
		end = chrono::steady_clock::now();
		duration = chrono::duration_cast<chrono::microseconds>(end - start);
		cout << "std::vector operator=:     " << double(duration.count()) << " us" << endl;


		start = chrono::steady_clock::now();
		vec3.insert(vec3.begin(), vec1.begin(), vec1.end());
		end = chrono::steady_clock::now();
		duration = chrono::duration_cast<chrono::microseconds>(end - start);
		cout << "std::vector insert:        " << double(duration.count()) << " us" << endl;
	}

	int runBenchMark() {
		funcHashMap();
		return 0;
	}

}