#ifndef _RAR_ARRAY_
#define _RAR_ARRAY_

template <class T> class Array
{
private:
	T *Buffer;
	int BufSize;
	int AllocSize;
public:
	Rar_Error_Handler& ErrHandler;
	Array(Rar_Error_Handler*);
	Array(int Size,Rar_Error_Handler*);
	~Array();
	inline void CleanData();
    inline T& operator [](int Item);
    inline int Size();
	void Add(int Items);
	void Alloc(int Items);
	void Reset();
    void operator = (Array<T> &Src);
	void Push(T Item);
    T* Addr() {return(Buffer);}
};

template <class T> void Array<T>::CleanData()
{
	Buffer=NULL;
	BufSize=0;
	AllocSize=0;
}


template <class T> Array<T>::Array(Rar_Error_Handler* eh) : ErrHandler( *eh )
{
	CleanData();
}


template <class T> Array<T>::Array(int Size, Rar_Error_Handler* eh) : ErrHandler( *eh )
{
	Buffer=(T *)rarmalloc(sizeof(T)*Size);
	if (Buffer==NULL && Size!=0)
		ErrHandler.MemoryError();

	AllocSize=BufSize=Size;
}


template <class T> Array<T>::~Array()
{
  if (Buffer!=NULL)
    rarfree(Buffer);
}


template <class T> inline T& Array<T>::operator [](int Item)
{
  return(Buffer[Item]);
}


template <class T> inline int Array<T>::Size()
{
  return(BufSize);
}


template <class T> void Array<T>::Add(int Items)
{
	int BufSize = this->BufSize; // don't change actual vars until alloc succeeds
	T*  Buffer  = this->Buffer;
	
	BufSize+=Items;
	if (BufSize>AllocSize)
	{
		int Suggested=AllocSize+AllocSize/4+32;
		int NewSize=Max(BufSize,Suggested);

		Buffer=(T *)rarrealloc(Buffer,NewSize*sizeof(T));
		if (Buffer==NULL)
			ErrHandler.MemoryError();
		AllocSize=NewSize;
	}
	
	this->Buffer  = Buffer;
	this->BufSize = BufSize;
}


template <class T> void Array<T>::Alloc(int Items)
{
	if (Items>AllocSize)
		Add(Items-BufSize);
	else
		BufSize=Items;
}


template <class T> void Array<T>::Reset()
{
	// Keep memory allocated if it's small
	// Eliminates constant reallocation when scanning archive
	if ( AllocSize < 1024/sizeof(T) )
	{
		BufSize = 0;
		return;
	}
	
	if (Buffer!=NULL)
	{
		rarfree(Buffer);
		Buffer=NULL;
	}
	BufSize=0;
	AllocSize=0;
}


template <class T> void Array<T>::operator =(Array<T> &Src)
{
  Reset();
  Alloc(Src.BufSize);
  if (Src.BufSize!=0)
    memcpy((void *)Buffer,(void *)Src.Buffer,Src.BufSize*sizeof(T));
}


template <class T> void Array<T>::Push(T Item)
{
	Add(1);
	(*this)[Size()-1]=Item;
}

#endif
