#pragma once
#include <QtCore>
#include <mutex>

#define FILEHEAD_LENGTH 100 //file head length

namespace memoryMappingFileBenchmark {

	template <class T>
	class VectorMMF
	{
	public:
		QString m_strFile;			//file name
		QFile m_file;				//file object
		uchar* m_pMap;				//memory-file map pointer
		T* m_pData;					//current data pointer
		int m_nIndexStart;			//current start index
		int m_nSizeType;			//size of data type 
		int m_nGrow;				//growth size value
		int m_nSize;				//elements number
		int m_nCapacity;			//capacity
		std::recursive_mutex m_mutex;
	public:
		VectorMMF(int nGrow = 5000) {
			m_nSizeType = sizeof(T);
			m_nGrow = nGrow;
		}
		void read(const QString& file) {
			m_nSizeType = sizeof(T);
			m_nSize = 0;
			m_strFile = file;
			m_nCapacity = m_nGrow;
			m_pMap = nullptr;
			m_nIndexStart = 0;
			//m_fileRw open
			m_file.setFileName(file);
			if (!m_file.open(QIODevice::ReadWrite)) {
				return;
			}
			char strOld[FILEHEAD_LENGTH];
			if (m_file.read(strOld, FILEHEAD_LENGTH) == FILEHEAD_LENGTH) {
				QString strKey = QString("V1.0.0_%1").arg(m_nSizeType, 4, 10, QChar(' '));//check file head
				if (QString(strOld) == strKey) {
					m_nSize = *(int*)(strOld + 94);
					m_nCapacity = *(int*)(strOld + 96);
				}
				else {
					m_file.resize(FILEHEAD_LENGTH);	//ËõÐ¡ÎÄ¼þ	
				}
			}
			reserve(m_nCapacity);
			remap(0);
		};
		void push_back(const T& item) {
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			if (m_nSize >= m_nCapacity) {
				reserve(m_nCapacity * 2);
			}
			int nNewIndexStart = m_nSize / m_nGrow * m_nGrow;
			if (nNewIndexStart != m_nIndexStart) {
				remap(nNewIndexStart);
			}
			m_pData[m_nSize - m_nIndexStart] = item;
			m_nSize++;
		}

		void resize(int nSize) {
			if (nSize < 0) {
				return;
			}
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			if (nSize > m_nCapacity) {
				reserve(nSize % m_nCapacity ? (nSize / m_nCapacity + 1) * m_nCapacity : nSize);
			}
			m_nSize = nSize;
			remap(0);
		}

		T& operator[] (const int nIndex) {
			Q_ASSERT(nIndex > -1 && nIndex < m_nSize);
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			int nNewIndexStart = nIndex / m_nGrow * m_nGrow;
			if (nNewIndexStart != m_nIndexStart) {
				remap(nNewIndexStart);
			}
			return m_pData[nIndex - m_nIndexStart];
		}
		void singleGet(const int nIndex, T& dest) {
			Q_ASSERT(nIndex > -1 && nIndex < m_nSize);
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			int nNewIndexStart = nIndex / m_nGrow * m_nGrow;
			if (nNewIndexStart != m_nIndexStart) {
				remap(nNewIndexStart);
			}
			dest = m_pData[nIndex - m_nIndexStart];
		}
		void batchGet(const int nFromIdx, std::vector<T>& toVec) {
			Q_ASSERT(nFromIdx > -1 && nFromIdx < m_nSize);
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			int nNewIndexStart = nFromIdx / m_nGrow * m_nGrow;
			if (nNewIndexStart != m_nIndexStart) {
				remap(nNewIndexStart);
			}
			int count = min(m_nSize - nFromIdx, m_nGrow);
			toVec.reserve(count);
			for (int i = 0; i < count; i++) {
				toVec.emplace_back(m_pData[i]);
			}
		}
		///save file
		void write() {
			char strNew[FILEHEAD_LENGTH] = { 0 };
			QString strKey = QString("V1.0.0_%1").arg(m_nSizeType, 4, 10, QChar(' '));
			strcpy(strNew, strKey.toUtf8().constData());
			memcpy(strNew + 94, &m_nSize, 4);
			memcpy(strNew + 96, &m_nCapacity, 4);

			if (m_pMap) {
				m_file.unmap(m_pMap);
			}
			///rewrite file head
			if (!m_file.isOpen() && !m_file.open(QIODevice::ReadWrite)) {
				return;
			}
			if (m_pMap = m_file.map(0, FILEHEAD_LENGTH)) {
				memcpy(m_pMap, strNew, FILEHEAD_LENGTH);
				m_file.unmap(m_pMap);
			}
			m_file.close();
		}
		void clear() {
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			m_nSize = 0;
			m_file.close();
		}
		int size() {
			return m_nSize;
		}
		~VectorMMF() {
			m_file.close();
		};

	private:
		void remap(int nIdxStart) {
			m_nIndexStart = nIdxStart;
			if (m_pMap) {
				m_file.unmap(m_pMap);
				m_pMap = nullptr;
			}
			qulonglong nFileOffset = ((qulonglong)m_nIndexStart) * m_nSizeType + FILEHEAD_LENGTH;
			if (!m_file.isOpen() && !m_file.open(QIODevice::ReadWrite)) {
				return;
			}
			m_pMap = m_file.map(nFileOffset, m_nGrow * m_nSizeType);
			m_file.close();
			m_pData = (T*)m_pMap;
		}
		void reserve(int nMinSize) {
			m_nCapacity = nMinSize;
			if (m_pMap) {
				m_file.unmap(m_pMap);
				m_pMap = nullptr;
			}
			///unsigned long 
			if (m_file.size() < (m_nCapacity * m_nSizeType + FILEHEAD_LENGTH)) {
				m_file.resize(m_nCapacity * m_nSizeType + FILEHEAD_LENGTH);
			}
		}
	};

	struct TestStruct
	{
		int mInt;
		double mDouble;
		char mCharArr[13];
		char mChar;
		bool mBool;
		TestStruct() {}
		TestStruct& operator=(const TestStruct& other) {
			mInt = other.mInt;
			mDouble = other.mDouble;
			mChar = other.mChar;
			mBool = other.mBool;
			memcpy(mCharArr, other.mCharArr, sizeof(mCharArr));
			return *this;
		}
		TestStruct(int n) :mInt(n), mDouble(n * 2.0), mBool(true), mChar('C'), mCharArr("TestStruct") {
		}
	};
	QString file("data.txt");
	VectorMMF<TestStruct> vectorMMF;
	const int g_repeatCount = 100000;
	int g_count = 0;

	void singleReadFunc() {
		TestStruct tstruct;
		int nSize = vectorMMF.size();
		for (int j = 0; j < nSize; j++) {
			vectorMMF.singleGet(j, tstruct);
		}
	}
	void batchReadFunc() {
		std::vector<TestStruct> tsVec;
		int nSize = vectorMMF.size();
		for (int i = 0; i < nSize; ) {
			vectorMMF.batchGet(i, tsVec);
			i += tsVec.size();
			tsVec.clear();
		}
	}
	void writeFunc() {
		vectorMMF.push_back(TestStruct(g_count++));
	}
	void threadFuncSingleRead() {
		static const int slround = 10;
		for (int i = 0; i < g_repeatCount / slround; i++) {
			singleReadFunc();
			std::this_thread::sleep_for(std::chrono::microseconds(slround));
		}
	}
	void threadFuncBatchRead() {
		static const int slround = 10;
		for (int i = 0; i < g_repeatCount / slround; i++) {
			batchReadFunc();
			std::this_thread::sleep_for(std::chrono::microseconds(slround));
		}
	}
	void threadFuncWrite() {
		for (int i = 0; i < g_repeatCount; i++) {
			writeFunc();
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
	void testRound() {
		std::cout << "repeat times:" << g_repeatCount << std::endl;

		std::vector<std::thread> threads;
		threads.clear();
		g_count = 0;
		vectorMMF.clear();
		if (QFile::exists(file)) {
			QFile::remove(file);
		}
		vectorMMF.read(file);
		auto startSingleRead = std::chrono::high_resolution_clock::now();
		threads.push_back(std::thread(threadFuncWrite));
		threads.push_back(std::thread(threadFuncSingleRead));
		for (auto& thread : threads)
			thread.join();
		auto endSingleRead = std::chrono::high_resolution_clock::now();
		std::cout << "single read:   " << std::chrono::duration_cast<std::chrono::milliseconds>(endSingleRead - startSingleRead).count() << " ms" << std::endl;
		vectorMMF.write();
		//////
		threads.clear();
		g_count = 0;
		vectorMMF.clear();
		if (QFile::exists(file)) {
			QFile::remove(file);
		}
		vectorMMF.read(file);
		auto startBatchRead = std::chrono::high_resolution_clock::now();
		threads.push_back(std::thread(threadFuncWrite));
		threads.push_back(std::thread(threadFuncBatchRead));
		for (auto& thread : threads)
			thread.join();
		auto endBatchRead = std::chrono::high_resolution_clock::now();
		std::cout << "batch read:    " << std::chrono::duration_cast<std::chrono::milliseconds>(endBatchRead - startBatchRead).count() << " ms" << std::endl;
		vectorMMF.write();
	}
	int runBenchMark() {
		testRound();
		return 0;
	}
}