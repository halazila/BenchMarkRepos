#pragma once
#include <QtCore>
#include <mutex>
#include "cleanSeprateReadWriteMMF.h"

#ifndef FILEHEAD_LENGTH
#define FILEHEAD_LENGTH 100 //file head length
#endif // !FILEHEAD_LENGTH


namespace memoryMappingFileBenchmark {
	using namespace std;
	///Window File Handle realize
#ifdef _MSC_VER
	template<class T>
	class CVectorMMF
	{
	public:
		std::wstring m_strFile;//文件名称	
		HANDLE m_hFile;//文件句柄
		HANDLE m_hFileMap;//文件映射
		void* m_pViewMap;//当前mapview指针
		T* m_pData;//当前数据指针
		int m_nIndexStart;//当前视图的起始index
		int m_nSizeType;//类型大小
		int m_nAllocGran;//分配粒度
		int m_nGrow;//每次递增值
		int m_nSize;//当前item个数
		int m_nCapacity;//最大个数
		bool m_bMapAll;//是否映射全部内存
		bool m_bTradeDate;//版本号是否带交易号
		map<int, char> m_mapEraseIndex;//已删掉的索引
		std::recursive_mutex m_mutex;
	public:
		CVectorMMF(int nGrow = 5000, bool bMapAll = false)//如果需要排序,m_bMapAll必须为true
		{
			m_nSizeType = sizeof(T);
			SYSTEM_INFO sysInfo;
			GetSystemInfo(&sysInfo);
			m_nAllocGran = sysInfo.dwAllocationGranularity;
			m_nGrow = nGrow;
			m_bMapAll = bMapAll;
		}
		CVectorMMF(CVectorMMF<T>& vc)
		{
			m_nSizeType = vc.m_nSizeType;
			m_nAllocGran = vc.m_nAllocGran;
			m_nGrow = vc.m_nGrow;
			m_nSize = vc.m_nSize;
			m_mapEraseIndex = vc.m_mapEraseIndex;
			m_hFileMap = NULL;
			m_hFile = CreateFile(m_strFile, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
			CopyFile(vc.m_strFile, m_strFile, FALSE);
			reserve(vc.m_nCapacity);
			remap(0);
		}

		~CVectorMMF()
		{
			CloseHandles();
		}

		void Read(std::wstring strFile, bool bTradeDate = true)//读文件头,防止down后数据全部要重传，所以不采用多线程版的写"unwrite"
		{
			m_nSize = 0;
			m_nCapacity = m_nGrow;
			m_pViewMap = NULL;
			m_hFileMap = NULL;
			m_bTradeDate = bTradeDate;
			m_mapEraseIndex.clear();

			m_strFile = strFile;
			m_hFile = CreateFile(m_strFile.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
			if (INVALID_HANDLE_VALUE == m_hFile)//出错则直接退出
			{
				exit(1);
			}

			char strOld[FILEHEAD_LENGTH];
			DWORD nRead;
			SetFilePointer(m_hFile, 0, NULL, FILE_BEGIN);

			if (ReadFile(m_hFile, strOld, FILEHEAD_LENGTH, &nRead, NULL) && nRead == FILEHEAD_LENGTH) {
				char wc[20];
				std::snprintf(wc, sizeof(wc), "V1.0.0_%4d", m_nSizeType);
				std::string strKey(wc);

				if (std::string(strOld) == strKey) {
					m_nSize = *(int*)(strOld + 92);
					m_nCapacity = *(int*)(strOld + 96);
				}
				else {
					SetEndOfFile(m_hFile);//缩小文件
				}
			}

			reserve(m_nCapacity);
			remap(0);
		}

		void write(bool bWrite = true)//写文件头
		{
			if (bWrite) {
				if (m_mapEraseIndex.size())//重新整理未删掉的数据
				{
					bool b;
					if (!(b = m_bMapAll)) {
						SetMapAll();
					}

					int nIndex = m_mapEraseIndex[0];
					for (int i = nIndex; i < m_nSize; i++) {
						if (m_mapEraseIndex.find(i) == m_mapEraseIndex.end()) {
							memmove(m_pData + nIndex, m_pData + i, m_nSizeType);
							nIndex++;
						}
					}

					if (!b) {
						SetMapAll(false);
					}
					m_nSize = nIndex;
				}

				char strNew[FILEHEAD_LENGTH];
				char wc[20];
				std::snprintf(wc, sizeof(wc), "V1.0.0_%4d", m_nSizeType);
				strcpy(strNew, wc);
				memcpy(strNew + 92, &m_nSize, 4);
				memcpy(strNew + 96, &m_nCapacity, 4);

				DWORD nWrite;
				SetFilePointer(m_hFile, 0, NULL, FILE_BEGIN);
				if (!WriteFile(m_hFile, strNew, FILEHEAD_LENGTH, &nWrite, NULL) || nWrite != FILEHEAD_LENGTH) {
					SetFilePointer(m_hFile, 0, NULL, FILE_BEGIN);
					SetEndOfFile(m_hFile);
				}
			}
			m_mapEraseIndex.clear();
			CloseHandles();
		}
		void push_back(const T& item)
		{
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			if (m_nSize >= m_nCapacity) {
				reserve(m_nCapacity * 2);
				if (m_bMapAll) {
					remap(0);
				}
			}

			if (!m_bMapAll) {
				int nNewIndexStart = m_nSize / m_nGrow * m_nGrow;
				if (nNewIndexStart != m_nIndexStart) {
					remap(nNewIndexStart);
				}
			}

			m_pData[m_nSize - m_nIndexStart] = item;
			m_nSize++;
		}

		void resize(int nSize)
		{
			if (nSize < 0) {
				return;
			}

			if (nSize > m_nCapacity) {
				reserve(nSize % m_nCapacity ? (nSize / m_nCapacity + 1) * m_nCapacity : nSize);
			}
			m_nSize = nSize;
			remap(0);
		}

		T& operator[] (const int nIndex)
		{
			//ASSERT(nIndex > -1 && nIndex < m_nSize);
			if (!m_hFile)//如果已经关闭，则返回一个空值罢
			{
				static T t = { 0 };
				return t;
			}
			if (!m_bMapAll) {
				int nNewIndexStart = nIndex / m_nGrow * m_nGrow;
				if (nNewIndexStart != m_nIndexStart) {
					remap(nNewIndexStart);
				}
			}
			return m_pData[nIndex - m_nIndexStart];
		}
		void singleGet(const int nIndex, T& dest) {
			//Q_ASSERT(nIndex > -1 && nIndex < m_nSize);
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			if (!m_hFile)//如果已经关闭，则返回一个空值罢
			{
				static T t = { 0 };
				dest = t;
				return;
			}
			int nNewIndexStart = nIndex / m_nGrow * m_nGrow;
			if (nNewIndexStart != m_nIndexStart) {
				remap(nNewIndexStart);
			}
			dest = m_pData[nIndex - m_nIndexStart];
		}
		bool operator== (CVectorMMF<T>& vc)
		{
			if (m_nSize != vc.size()) {
				return false;
			}

			bool b1, b2;
			if (!(b1 = m_bMapAll)) {
				SetMapAll();
			}
			if (!(b2 = vc.m_bMapAll)) {
				vc.SetMapAll();
			}

			bool bResult = !memcmp(m_pData, vc.m_pData, m_nSize * m_nSizeType);

			if (!b1) {
				SetMapAll(false);
			}
			if (!b2) {
				vc.SetMapAll(false);
			}

			return bResult;
		}
		bool operator!= (CVectorMMF<T>& vc)
		{
			return !((*this) == vc);
		}

		CVectorMMF<T>& operator= (const CVectorMMF<T>& vc)
		{
			m_nSize = vc.m_nSize;
			m_mapEraseIndex = vc.m_mapEraseIndex;
			if (m_pViewMap) {
				UnmapViewOfFile(m_pViewMap);
				m_pViewMap = NULL;
			}
			if (m_hFileMap) {
				CloseHandle(m_hFileMap);
				m_hFileMap = NULL;
			}
			CopyFile(vc.m_strFile, m_strFile, FALSE);

			reserve(vc.m_nCapacity);
			remap(0);
			return *this;

		}
		void erase(int nIndex)//可能会造成进程地址空间不足，尽量不要使用
		{
			if (nIndex < 0 || nIndex >= m_nSize) {
				return;
			}

			if (nIndex != m_nSize - 1) {
				bool b;
				if (!(b = m_bMapAll)) {
					SetMapAll();
				}
				memmove(m_pData + nIndex, m_pData + nIndex + 1, m_nSizeType * (m_nSize - nIndex - 1));
				if (!b) {
					SetMapAll(false);
				}
			}
			m_nSize--;
		}

		void erase_virtual(int nIndex)
		{
			if (nIndex < 0 || nIndex >= m_nSize) {
				return;
			}
			m_mapEraseIndex[nIndex] = 0;
		}
		bool validate_index(int nIndex)
		{
			return m_mapEraseIndex.size() == 0 || m_mapEraseIndex.find(nIndex) == m_mapEraseIndex.end();
		}
		void clear(bool bCloseHandle = false)
		{
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			m_nSize = 0;
			m_mapEraseIndex.clear();
			if (bCloseHandle) {
				CloseHandles();
			}
		}

		int size()
		{
			return m_nSize;
		}
		void remap(int IndexStart)
		{
			m_nIndexStart = IndexStart;
			if (m_pViewMap) {
				UnmapViewOfFile(m_pViewMap);
				m_pViewMap = NULL;
			}

			ULONGLONG nFileOffset = ((ULONGLONG)m_nIndexStart) * m_nSizeType + FILEHEAD_LENGTH;
			DWORD dwFileOffsetHigh = nFileOffset >> 32;
			DWORD dwFileOffsetLow = nFileOffset & 0xFFFFFFFF;

			int nMod = nFileOffset % m_nAllocGran;

			if (!m_bMapAll) {
				m_pViewMap = MapViewOfFile(m_hFileMap, FILE_MAP_WRITE, dwFileOffsetHigh, dwFileOffsetLow - nMod, m_nGrow * m_nSizeType + nMod);
			}
			else {
				m_pViewMap = MapViewOfFile(m_hFileMap, FILE_MAP_WRITE, 0, FILEHEAD_LENGTH - nMod, 0);
			}

			m_pData = (T*)((char*)m_pViewMap + nMod);
		}

		void reserve(int nMinSize)
		{
			m_nCapacity = nMinSize;
			if (m_pViewMap) {
				UnmapViewOfFile(m_pViewMap);
				m_pViewMap = NULL;
			}
			if (m_hFileMap) {
				CloseHandle(m_hFileMap);
			}

			ULONGLONG nMaximumSize = ((ULONGLONG)m_nCapacity) * m_nSizeType + FILEHEAD_LENGTH;
			DWORD dwMaximumSizeHigh = nMaximumSize >> 32;
			DWORD dwMaximumSizeLow = nMaximumSize & 0xFFFFFFFF;
			m_hFileMap = CreateFileMapping(m_hFile, NULL, PAGE_READWRITE, dwMaximumSizeHigh, dwMaximumSizeLow, NULL);
		}

		void SetMapAll(bool bMapAll = true)
		{
			m_bMapAll = bMapAll;
			remap(0);
		}

		void PushBuffer(T* buf, int nCount)//直接写入一个buf,也就是nCount个item
		{
			if (m_nSize + nCount > m_nCapacity) {
				reserve((m_nSize + nCount) / m_nGrow * m_nGrow + ((m_nSize + nCount) % m_nGrow ? m_nGrow : 0));
			}

			bool b;
			if (!(b = m_bMapAll)) {
				SetMapAll();
			}
			memcpy(m_pData + m_nSize, buf, m_nSizeType * nCount);
			m_nSize += nCount;
			if (!b) {
				SetMapAll(false);
			}
		}

		void ReleaseVirtualMemory(void) //释放多余的进程空间
		{
			if (m_bMapAll) //仅在全部映射情况下才如此做
			{
				if (m_nCapacity > (m_nSize / m_nGrow + 1) * m_nGrow) {
					m_nCapacity = (m_nSize / m_nGrow + 1) * m_nGrow;
					remap(0);
				}
			}
		}

		void CloseHandles(void)//关闭各句柄
		{
			if (m_pViewMap) {
				UnmapViewOfFile(m_pViewMap);
				m_pViewMap = NULL;
			}
			if (m_hFileMap) {
				CloseHandle(m_hFileMap);
				m_hFileMap = NULL;
			}

			if (m_hFile) {
				CloseHandle(m_hFile);
				m_hFile = NULL;
			}
		}
	};
#endif // _MSC_VER


	///QFile map realize
	template <class T>
	class VectorMMF
	{
	public:
		///common attr
		QString m_strFile;			//file name
		int m_nSizeType;			//size of data type 
		int m_nGrow;				//growth size value
		int m_nSize;				//elements number
		int m_nCapacity;			//capacity
		///write handle
		QFile m_fileWrite;			//file object
		uchar* m_pMapWrite;			//memory-file map pointer
		T* m_pDataWrite;			//current data pointer
		int m_nIndexStartWrite;		//current start index
		///read handle
		QFile m_fileRead;
		uchar* m_pMapRead;
		T* m_pDataRead;
		int m_nIndexStartRead;
		///mutex
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
			m_pMapWrite = nullptr;
			m_nIndexStartWrite = 0;
			//m_fileRw open
			m_fileWrite.setFileName(file);
			if (!m_fileWrite.open(QIODevice::ReadWrite)) {
				return;
			}
			m_fileRead.setFileName(file);
			if (!m_fileRead.open(QIODevice::ReadOnly)) {
				return;
			}
			char strOld[FILEHEAD_LENGTH];
			if (m_fileWrite.read(strOld, FILEHEAD_LENGTH) == FILEHEAD_LENGTH) {
				QString strKey = QString("V1.0.0_%1").arg(m_nSizeType, 4, 10, QChar(' '));//check file head
				if (QString(strOld) == strKey) {
					m_nSize = *(int*)(strOld + 94);
					m_nCapacity = *(int*)(strOld + 96);
				}
				else {
					m_fileWrite.resize(FILEHEAD_LENGTH);	//缩小文件	
				}
			}
			reserve(m_nCapacity);
			remapWrite(0);
			remapRead(0);
		};
		void push_back(const T& item) {
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			if (m_nSize >= m_nCapacity) {
				reserve(m_nCapacity * 2);
			}
			int nNewIndexStart = m_nSize / m_nGrow * m_nGrow;
			if (nNewIndexStart != m_nIndexStartWrite) {
				remapWrite(nNewIndexStart);
			}
			m_pDataWrite[m_nSize - m_nIndexStartWrite] = item;
			m_nSize++;
		}

		void resize(int nSize) {
			if (nSize < 0) {
				return;
			}
			///std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			if (nSize > m_nCapacity) {
				reserve(nSize % m_nCapacity ? (nSize / m_nCapacity + 1) * m_nCapacity : nSize);
			}
			m_nSize = nSize;
			remapWrite(0);
		}

		T& operator[] (const int nIndex) {
			Q_ASSERT(nIndex > -1 && nIndex < m_nSize);
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			int nNewIndexStart = nIndex / m_nGrow * m_nGrow;
			if (nNewIndexStart != m_nIndexStartWrite) {
				remapWrite(nNewIndexStart);
			}
			return m_pDataWrite[nIndex - m_nIndexStartWrite];
		}
		void singleGet(const int nIndex, T& dest) {
			Q_ASSERT(nIndex > -1 && nIndex < m_nSize);
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			int nNewIndexStart = nIndex / m_nGrow * m_nGrow;
			if (nNewIndexStart != m_nIndexStartWrite) {
				remapWrite(nNewIndexStart);
			}
			dest = m_pDataWrite[nIndex - m_nIndexStartWrite];
		}
		void singleGetReadOnly(const int nIndex, T& dest) {
			Q_ASSERT(nIndex > -1 && nIndex < m_nSize);
			int nNewIndexStart = nIndex / m_nGrow * m_nGrow;
			if (nNewIndexStart != m_nIndexStartRead) {
				remapRead(nNewIndexStart);
			}
			dest = m_pDataRead[nIndex - m_nIndexStartRead];
		}
		void batchGet(const int nFromIdx, std::vector<T>& toVec) {
			Q_ASSERT(nFromIdx > -1 && nFromIdx < m_nSize);
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			int nNewIndexStart = nFromIdx / m_nGrow * m_nGrow;
			if (nNewIndexStart != m_nIndexStartWrite) {
				remapWrite(nNewIndexStart);
			}
			int count = min(m_nSize - nFromIdx, m_nGrow);
			toVec.reserve(count);
			for (int i = 0; i < count; i++) {
				toVec.emplace_back(m_pDataWrite[i]);
			}
		}
		int batchGet(const int nFromIdx, T* dest) {
			Q_ASSERT(nFromIdx > -1 && nFromIdx < m_nSize);
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			int nNewIndexStart = nFromIdx / m_nGrow * m_nGrow;
			if (nNewIndexStart != m_nIndexStartWrite) {
				remapWrite(nNewIndexStart);
			}
			int count = min(m_nSize - nFromIdx, m_nGrow);
			memcpy(dest, m_pDataWrite, count * m_nSizeType);
			return count;
		}
		///save file
		void write() {
			if (m_pMapRead) {
				m_fileRead.unmap(m_pMapRead);
				m_pMapRead = nullptr;
				m_pDataRead = nullptr;
			}
			m_fileRead.close();

			char strNew[FILEHEAD_LENGTH] = { 0 };
			QString strKey = QString("V1.0.0_%1").arg(m_nSizeType, 4, 10, QChar(' '));
			strcpy(strNew, strKey.toUtf8().constData());
			memcpy(strNew + 94, &m_nSize, 4);
			memcpy(strNew + 96, &m_nCapacity, 4);

			if (m_pMapWrite) {
				m_fileWrite.unmap(m_pMapWrite);
				m_pMapWrite = nullptr;
			}
			///rewrite file head
			if (m_pMapWrite = m_fileWrite.map(0, FILEHEAD_LENGTH)) {
				memcpy(m_pMapWrite, strNew, FILEHEAD_LENGTH);
				m_fileWrite.unmap(m_pMapWrite);
				m_pMapWrite = nullptr;
				m_pDataWrite = nullptr;
			}
			m_fileWrite.close();
		}
		void clear() {
			std::lock_guard<decltype(m_mutex)> lock(m_mutex);
			m_nSize = 0;
			if (m_pMapWrite) {
				m_fileWrite.unmap(m_pMapWrite);
				m_pMapWrite = nullptr;
				m_pDataWrite = nullptr;
			}
			m_fileWrite.close();
			if (m_pMapRead) {
				m_fileRead.unmap(m_pMapRead);
				m_pMapRead = nullptr;
				m_pDataRead = nullptr;
			}
			m_fileRead.close();
		}
		int size() {
			return m_nSize;
		}
		~VectorMMF() {
			if (m_pMapWrite) {
				m_fileWrite.unmap(m_pMapWrite);
				m_pMapWrite = nullptr;
				m_pDataWrite = nullptr;
			}
			m_fileWrite.close();
			if (m_pMapRead) {
				m_fileRead.unmap(m_pMapRead);
				m_pMapRead = nullptr;
				m_pDataRead = nullptr;
			}
			m_fileRead.close();
		};

	private:
		void remapWrite(int nIdxStart) {
			m_nIndexStartWrite = nIdxStart;
			if (m_pMapWrite) {
				m_fileWrite.unmap(m_pMapWrite);
				m_pMapWrite = nullptr;
			}
			qulonglong nFileOffset = ((qulonglong)m_nIndexStartWrite) * m_nSizeType + FILEHEAD_LENGTH;
			m_pMapWrite = m_fileWrite.map(nFileOffset, m_nGrow * m_nSizeType);
			m_pDataWrite = (T*)m_pMapWrite;
		}
		void remapRead(int nIdxStart) {
			m_nIndexStartRead = nIdxStart;
			qulonglong nFileOffset = ((qulonglong)nIdxStart) * m_nSizeType + FILEHEAD_LENGTH;
			if (m_pMapRead) {
				m_fileRead.unmap(m_pMapRead);
				m_pMapRead = nullptr;
			}
			m_pMapRead = m_fileRead.map(nFileOffset, m_nGrow * m_nSizeType);
			m_pDataRead = (T*)m_pMapRead;
		}
		void reserve(int nMinSize) {
			m_nCapacity = nMinSize;
			if (m_pMapWrite) {
				m_fileWrite.unmap(m_pMapWrite);
				m_pMapWrite = nullptr;
			}
			///unsigned long 
			if (m_fileWrite.size() < (m_nCapacity * m_nSizeType + FILEHEAD_LENGTH)) {
				m_fileWrite.resize(m_nCapacity * m_nSizeType + FILEHEAD_LENGTH);
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
	std::wstring filews(L"data.txt");
	VectorMMF<TestStruct> vectorMMF;
	CVectorMMF<TestStruct> winVecMMF;
	const int g_repeatCount = 1000000;
	int g_count = 0;
	double g_double;
	const int g_slround = 1000;

#ifdef _MSC_VER
	void singleWinReadFunc() {
		TestStruct tstruct;
		int nSize = winVecMMF.size();
		for (int j = 0; j < nSize; j++) {
			winVecMMF.singleGet(j, tstruct);
			tstruct.mInt *= 2;
			g_double = sqrt(tstruct.mInt);
		}
	}
	void writeWinFunc() {
		winVecMMF.push_back(TestStruct(g_count++));
	}
	void threadFuncSingleWinRead() {
		for (int i = 0; i < g_repeatCount / g_slround; i++) {
			singleWinReadFunc();
			std::this_thread::sleep_for(std::chrono::microseconds(g_slround));
		}
	}
	void threadFuncWinWrite() {
		for (int i = 0; i < g_repeatCount; i++) {
			writeWinFunc();
		}
	}
#endif // _MSC_VER



	void singleReadFunc() {
		TestStruct tstruct;
		int nSize = vectorMMF.size();
		for (int j = 0; j < nSize; j++) {
			vectorMMF.singleGet(j, tstruct);
			tstruct.mInt *= 2;
			g_double = sqrt(tstruct.mInt);
		}
	}
	void singleReadOnlyFunc() {
		TestStruct tstruct;
		int nSize = vectorMMF.size();
		for (int j = 0; j < nSize; j++) {
			vectorMMF.singleGetReadOnly(j, tstruct);
			tstruct.mInt *= 2;
			g_double = sqrt(tstruct.mInt);
		}
	}
	void batchReadFuncArr(TestStruct* tsarr) {
		int nSize = vectorMMF.size();
		for (int i = 0; i < nSize; ) {
			int n = vectorMMF.batchGet(i, tsarr);
			i += n;
			do {
				tsarr[n - 1].mInt *= 2;
				g_double = sqrt(tsarr[n - 1].mInt);
			} while (--n);
		}
	}
	void batchReadFuncVec(std::vector<TestStruct>& tsVec) {
		;
		int nSize = vectorMMF.size();
		for (int i = 0; i < nSize; ) {
			vectorMMF.batchGet(i, tsVec);
			i += tsVec.size();
			for (int j = 0; j < tsVec.size(); j++) {
				tsVec[j].mInt *= 2;
				g_double = sqrt(tsVec[j].mInt);
			}
			tsVec.clear();
		}
	}
	void writeFunc() {
		vectorMMF.push_back(TestStruct(g_count++));
	}
	void threadFuncSingleRead() {
		for (int i = 0; i < g_repeatCount / g_slround; i++) {
			singleReadFunc();
			std::this_thread::sleep_for(std::chrono::microseconds(g_slround));
		}
	}
	void threadFuncSingleReadOnly() {
		for (int i = 0; i < g_repeatCount / g_slround; i++) {
			singleReadOnlyFunc();
			std::this_thread::sleep_for(std::chrono::microseconds(g_slround));
		}
	}
	void threadFuncBatchReadArr() {
		TestStruct* tsarr = new TestStruct[vectorMMF.m_nGrow];
		for (int i = 0; i < g_repeatCount / g_slround; i++) {
			batchReadFuncArr(tsarr);
			std::this_thread::sleep_for(std::chrono::microseconds(g_slround));
		}
		delete[] tsarr;
	}
	void threadFuncBatchReadVec() {
		std::vector<TestStruct> tsVec;
		for (int i = 0; i < g_repeatCount / g_slround; i++) {
			batchReadFuncVec(tsVec);
			std::this_thread::sleep_for(std::chrono::microseconds(g_slround));
			tsVec.clear();
		}
	}
	void threadFuncWrite() {
		for (int i = 0; i < g_repeatCount; i++) {
			writeFunc();
		}
	}

	///测试函数
	void testRound() {
		std::cout << "repeat times:" << g_repeatCount << std::endl;
		std::vector<std::thread> threads;

		decltype(std::chrono::high_resolution_clock::now())  startTime, endTime;

		///qfile map api，每次读取一个
		threads.clear();
		g_count = 0;
		vectorMMF.clear();
		if (QFile::exists(file)) {
			QFile::remove(file);
		}
		vectorMMF.read(file);
		startTime = std::chrono::high_resolution_clock::now();
		threads.push_back(std::thread(threadFuncWrite));
		threads.push_back(std::thread(threadFuncSingleRead));
		for (auto& thread : threads)
			thread.join();
		endTime = std::chrono::high_resolution_clock::now();
		std::cout << "single read:    " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << " ms" << std::endl;
		vectorMMF.write();
		std::cout << g_double << " \n\r";

		///wind memeory api,每次读取一个
		threads.clear();
		g_count = 0;
		winVecMMF.clear();
		if (QFile::exists(file)) {
			QFile::remove(file);
		}
		winVecMMF.Read(filews);
		startTime = std::chrono::high_resolution_clock::now();
		threads.push_back(std::thread(threadFuncWinWrite));
		threads.push_back(std::thread(threadFuncSingleWinRead));
		for (auto& thread : threads)
			thread.join();
		endTime = std::chrono::high_resolution_clock::now();
		std::cout << "single win read:" << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << " ms" << std::endl;
		winVecMMF.write();
		std::cout << g_double << " \n\r";

		///qfile map api，读写分离，二者同时map一个文件（存在脏读情况）
		threads.clear();
		g_count = 0;
		vectorMMF.clear();
		if (QFile::exists(file)) {
			QFile::remove(file);
		}
		vectorMMF.read(file);
		startTime = std::chrono::high_resolution_clock::now();
		threads.push_back(std::thread(threadFuncWrite));
		threads.push_back(std::thread(threadFuncSingleReadOnly));
		for (auto& thread : threads)
			thread.join();
		endTime = std::chrono::high_resolution_clock::now();
		std::cout << "s-only read:    " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << " ms" << std::endl;
		vectorMMF.write();
		std::cout << g_double << " \n\r";

		////qfile map api，用数组批量读取
		threads.clear();
		g_count = 0;
		vectorMMF.clear();
		if (QFile::exists(file)) {
			QFile::remove(file);
		}
		vectorMMF.read(file);
		startTime = std::chrono::high_resolution_clock::now();
		threads.push_back(std::thread(threadFuncWrite));
		threads.push_back(std::thread(threadFuncBatchReadArr));
		for (auto& thread : threads)
			thread.join();
		endTime = std::chrono::high_resolution_clock::now();
		std::cout << "arr batch read:     " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << " ms" << std::endl;
		vectorMMF.write();
		std::cout << g_double << " \n\r";

		////qfile map api，用vector批量读取
		threads.clear();
		g_count = 0;
		vectorMMF.clear();
		if (QFile::exists(file)) {
			QFile::remove(file);
		}
		vectorMMF.read(file);
		startTime = std::chrono::high_resolution_clock::now();
		threads.push_back(std::thread(threadFuncWrite));
		threads.push_back(std::thread(threadFuncBatchReadVec));
		for (auto& thread : threads)
			thread.join();
		endTime = std::chrono::high_resolution_clock::now();
		std::cout << "vec batch read:     " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << " ms" << std::endl;
		vectorMMF.write();
		std::cout << g_double << " \n\r";
	}
	int runBenchMark() {
		testRound();
		return 0;
	}
}