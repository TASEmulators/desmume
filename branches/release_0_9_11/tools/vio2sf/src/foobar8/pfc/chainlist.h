#ifndef _PFC_CHAINLIST_H_
#define _PFC_CHAINLIST_H_


template<class T>
class chain_list
{
	class elem
	{
	public:
		T item;
		elem * prev, * next;
		elem(const T& param,int blah) : item(param) {}
		elem(T param) : item(param) {}
		elem(int, int, int) {}
	};

	elem * first, * last;
	
public:
	class enumerator;

private:
	static inline elem * parse_enum(const enumerator & en) {assert(en.ptr);return reinterpret_cast<elem*>(en.ptr);}
	static inline void modify_enum(enumerator & en, elem * ptr) {en.ptr = reinterpret_cast<void*>(ptr);}

	void remove(elem * ptr)
	{
		assert(ptr);
		(ptr->prev ? ptr->prev->next : first) = ptr->next;
		(ptr->next ? ptr->next->prev : last) = ptr->prev;
		delete ptr;
	}

	void init_last(elem * ptr)
	{
		ptr->prev = last;
		ptr->next = 0;
		last = ptr;
		(ptr->prev ? ptr->prev->next : first) = ptr;
	}
	void init_first(elem * ptr)
	{
		ptr->next = first;
		ptr->prev = 0;
		first = ptr;
		(ptr->next ? ptr->next->prev : last) = ptr;
	}

public:
	inline ~chain_list() {remove_all();}
	inline explicit chain_list() {first = 0; last = 0;}

	inline void advance(enumerator & en) const
	{
		modify_enum(en,parse_enum(en)->next);
	}

	inline void reset(enumerator & en) const
	{
		modify_enum(en,first);
	}
	
	inline T get_item(const enumerator & en) const
	{
		return parse_enum(en)->item;
	}

	inline const T& get_item_ref(const enumerator & en) const
	{
		return parse_enum(en)->item;
	}
	
	inline T& get_item_var(const enumerator & en)
	{
		return parse_enum(en)->item;
	}

	void remove_all()
	{
		while(first) {remove(first);}
	}

	unsigned get_count() const
	{
		unsigned val = 0;
		elem * ptr = first;
		while(ptr) {val++; ptr = ptr->next;}
		return val;
	}

	inline void add_item_first(T item)
	{
		init_first(new elem(item));
	}

	inline void add_item_first_ref(const T & item)
	{
		init_first(new elem(item,0));
	}

	inline void add_item(T item)
	{
		init_last(new elem(item));
	}

	inline void add_item_ref(const T & item)
	{
		init_last(new elem(item,0));
	}

	void remove_item(enumerator & en)
	{
		elem * freeme = parse_enum(en);
		advance(en);
		remove(freeme);
	}

	inline T& add_item_noinit()
	{
		elem * ptr = new elem(0,0,0);
		init_last(ptr);
		return ptr->item;
	}

	inline T& add_item_first_noinit()
	{
		elem * ptr = new elem(0,0,0);
		init_first(ptr);
		return ptr->item;
	}
	
	class enumerator
	{
		const chain_list<T> & list;
		void * ptr;
		friend class chain_list<T>;
	public:
		explicit enumerator(const chain_list<T> & p_list) : list(p_list), ptr(0) {reset();}		

		inline void reset() {list.reset(*this);}
		inline void advance() {list.advance(*this);}
		
		inline T get_item() const {return list.get_item(*this);}
		inline const T& get_item_ref() const {return list.get_item(*this);}

		inline T first() {reset();return get_item();}
		inline const T& first_ref() {reset();return get_item_ref();}

		inline T next() {advance();return get_item();}
		inline const T& next_ref() {advance();return get_item_ref();}

		inline operator bool() const {return !!ptr;}
		
	};

};


#endif _PFC_CHAINLIST_H_