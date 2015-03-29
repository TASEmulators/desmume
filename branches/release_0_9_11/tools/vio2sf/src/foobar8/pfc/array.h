#ifndef _PFC_ARRAY_H_
#define _PFC_ARRAY_H_

template<class T>
class array_t
{
public:
	explicit array_t(unsigned size = 0)
	{
		m_data = 0;
		m_size = 0;
		resize(size);
	}

	inline const T& operator[](unsigned n) const
	{
		assert(n<m_size); assert(m_data);
		return m_data[n];
	}

	inline T& operator[](unsigned n)
	{
		assert(n<m_size); assert(m_data);
		return m_data[n];
	}
	
	bool resize(unsigned new_size)
	{
		if (new_size == m_size) //do nothing
		{
		}
		else if (new_size == 0)
		{
			if (m_data)
			{
				delete[] m_data;
				m_data = 0;
			}
			m_size = 0;
		}
		else
		{
			T* new_data = new T[new_size];
			if (new_data == 0) return false;
			if (m_data)
			{
				unsigned n,tocopy = new_size > m_size ? m_size : new_size;
				for(n=0;n<tocopy;n++)
					new_data[n] = m_data[n];
				delete[] m_data;
				
			}
			m_data = new_data;
			m_size = new_size;
		}
		return true;
	}


	inline unsigned size() const {return m_size;}

	~array_t()
	{
		if (m_data) delete m_data;
	}
	
private:
	T* m_data;
	unsigned m_size;
};


#endif //_PFC_ARRAY_H_