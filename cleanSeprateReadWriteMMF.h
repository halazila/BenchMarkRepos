#pragma once

#ifndef FILEHEAD_LENGTH
#define FILEHEAD_LENGTH 100 //file head length
#endif // !FILEHEAD_LENGTH


template <class T>
class CleanVectorMMF
{
public:
	///common attr
	QString m_strFile;			//file name
	int m_nSizeType;			//size of data type 
	int m_nGrow;				//growth size value
	int m_nSize;				//elements number
	int m_nCapacity;			//capacity
	///write handle
	QFile m_fileWrite;				//file object
	uchar* m_pMapWrite;				//memory-file map pointer
	T* m_pData;					//current data pointer
	int m_nIndexStartWrite;			//current start index
	///read handle
	QFile m_fileRead;
	uchar* m_pMapRead;
	T* m_pDataRead;
	int m_nIndexStartRead;
	///mutex
	std::recursive_mutex m_mutex;
	///atomic
	std::atomic<int> m_nWriteProtect;
	std::atomic<int> m_nReadProtect;
public:
	CleanVectorMMF(int nGrow = 5000) {
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
				m_fileWrite.resize(FILEHEAD_LENGTH);	//ËõÐ¡ÎÄ¼þ	
			}
		}
		reserve(m_nCapacity);
		remap(0);
		remapReadOnly(0);
	};

	T& operator[] (const int nIndex) {
		Q_ASSERT(nIndex > -1 && nIndex < m_nSize);
		std::lock_guard<decltype(m_mutex)> lock(m_mutex);
		int nNewIndexStart = nIndex / m_nGrow * m_nGrow;
		if (nNewIndexStart != m_nIndexStartWrite) {
			remap(nNewIndexStart);
		}
		return m_pData[nIndex - m_nIndexStartWrite];
	}

	void push_back(const T& item) {
		std::lock_guard<decltype(m_mutex)> lock(m_mutex);
		if (m_nSize >= m_nCapacity) {
			reserve(m_nCapacity * 2);
		}
		int nNewIndexStart = m_nSize / m_nGrow * m_nGrow;
		if (nNewIndexStart != m_nIndexStartWrite) {
			remap(nNewIndexStart);
		}
		m_pData[m_nSize - m_nIndexStartWrite] = item;
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
		remap(0);
	}

	void singleGetReadOnly(const int nIndex, T& dest) {
		Q_ASSERT(nIndex > -1 && nIndex < m_nSize);
		int nNewIndexStart = nIndex / m_nGrow * m_nGrow;
		if (nNewIndexStart != m_nIndexStartRead) {
			remapReadOnly(nNewIndexStart);
		}
		dest = m_pDataRead[nIndex - m_nIndexStartRead];
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
			m_pData = nullptr;
		}
		m_fileWrite.close();
	}
	void clear() {
		std::lock_guard<decltype(m_mutex)> lock(m_mutex);
		m_nSize = 0;
		if (m_pMapWrite) {
			m_fileWrite.unmap(m_pMapWrite);
			m_pMapWrite = nullptr;
			m_pData = nullptr;
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
	~CleanVectorMMF() {
		if (m_pMapWrite) {
			m_fileWrite.unmap(m_pMapWrite);
			m_pMapWrite = nullptr;
			m_pData = nullptr;
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
	void remap(int nIdxStart) {
		m_nIndexStartWrite = nIdxStart;
		if (m_pMapWrite) {
			m_fileWrite.unmap(m_pMapWrite);
			m_pMapWrite = nullptr;
		}
		qulonglong nFileOffset = ((qulonglong)m_nIndexStartWrite) * m_nSizeType + FILEHEAD_LENGTH;
		m_pMapWrite = m_fileWrite.map(nFileOffset, m_nGrow * m_nSizeType);
		m_pData = (T*)m_pMapWrite;
	}
	void remapReadOnly(int nIdxStart) {
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

