#ifndef _MEM_BLOCK_MGR_H_
#define _MEM_BLOCK_MGR_H_

#error DEPRECATED


template<class T>
class mem_block_manager
{
	struct entry
	{
		mem_block_t<T> block;
		bool used;
	};
	ptr_list_t<entry> list;
public:
	T * copy(const T* ptr,int size)
	{
		int n;
		int found_size = -1,found_index = -1;
		for(n=0;n<list.get_count();n++)
		{
			if (!list[n]->used)
			{
				int block_size = list[n]->block.get_size();
				if (found_size<0)
				{
					found_index=n; found_size = block_size;
				}
				else if (found_size<size)
				{
					if (block_size>found_size)
					{
						found_index=n; found_size = block_size;
					}
				}
				else if (found_size>size)
				{
					if (block_size>=size && block_size<found_size)
					{
						found_index=n; found_size = block_size;
					}
				}

				if (found_size==size) break;
			}
		}
		if (found_index>=0)
		{
			list[found_index]->used = true;
			return list[found_index]->block.copy(ptr,size);				
		}
		entry * new_entry = new entry;
		new_entry->used = true;
		list.add_item(new_entry);
		return new_entry->block.copy(ptr,size);
	}
	
	void mark_as_free()
	{
		int n;
		for(n=0;n<list.get_count();n++)
		{
			list[n]->used = false;
		}
	}

	~mem_block_manager() {list.delete_all();}
};

#endif