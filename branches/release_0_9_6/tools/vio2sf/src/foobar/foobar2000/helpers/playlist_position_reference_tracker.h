class playlist_position_reference_tracker : public playlist_callback_impl_base {
public:
	//! @param p_trackitem Specifies whether we want to track some specific item rather than just an offset in a playlist. When set to true, item index becomes invalidated when the item we're tracking is removed.
	playlist_position_reference_tracker(bool p_trackitem = true) : playlist_callback_impl_base(~0), m_trackitem(p_trackitem), m_playlist(infinite), m_item(infinite) {}
	
	void on_items_added(t_size p_playlist,t_size p_start, const pfc::list_base_const_t<metadb_handle_ptr> & p_data,const bit_array & p_selection) {
		if (p_playlist == m_playlist && m_item != infinite && p_start <= m_item) {
			m_item += p_data.get_count();
		}
	}
	void on_items_reordered(t_size p_playlist,const t_size * p_order,t_size p_count) {
		if (p_playlist == m_playlist) {
			if (m_item < p_count) {
				m_item = order_helper::g_find_reverse(p_order,m_item);
			} else {
				m_item = infinite;
			}
		}
	}

	void on_items_removed(t_size p_playlist,const bit_array & p_mask,t_size p_old_count,t_size p_new_count) {
		if (p_playlist == m_playlist) {
			if (m_item < p_old_count) {
				const t_size item_before = m_item;
				for(t_size walk = p_mask.find_first(true,0,p_old_count); walk < p_old_count; walk = p_mask.find_next(true,walk,p_old_count)) {
					if (walk < item_before) { 
						m_item--;
					} else if (walk == item_before) {
						if (m_trackitem) m_item = infinite;
						break;
					} else {
						break;
					}
				}
				if (m_item >= p_new_count) m_item = infinite;
			} else {
				m_item = infinite;
			}
		}
	}

	//todo? could be useful in some cases
	void on_items_replaced(t_size p_playlist,const bit_array & p_mask,const pfc::list_base_const_t<t_on_items_replaced_entry> & p_data) {}

	void on_playlist_created(t_size p_index,const char * p_name,t_size p_name_len) {
		if (m_playlist != infinite && p_index <= m_playlist) m_playlist++;
	}
	void on_playlists_reorder(const t_size * p_order,t_size p_count) {
		if (m_playlist < p_count) m_playlist = order_helper::g_find_reverse(p_order,m_playlist);
		else m_playlist = infinite;
	}
	void on_playlists_removed(const bit_array & p_mask,t_size p_old_count,t_size p_new_count) {
		if (m_playlist < p_old_count) {
			const t_size playlist_before = m_playlist;
			for(t_size walk = p_mask.find_first(true,0,p_old_count); walk < p_old_count; walk = p_mask.find_next(true,walk,p_old_count)) {
				if (walk < playlist_before) {
					m_playlist--;
				} else if (walk == playlist_before) {
					m_playlist = infinite;
					break;
				} else {
					break;
				}
			}
		} else {
			m_playlist = infinite;
		}
	}

	t_size m_playlist, m_item;
private:
	const bool m_trackitem;
};
