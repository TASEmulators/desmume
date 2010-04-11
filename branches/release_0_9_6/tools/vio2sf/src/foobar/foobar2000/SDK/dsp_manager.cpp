#include "foobar2000.h"

void dsp_manager::close() {
	m_chain.remove_all();
	m_config_changed = true;
}

void dsp_manager::set_config( const dsp_chain_config & p_data )
{
	//dsp_chain_config::g_instantiate(m_dsp_list,p_data);
	m_config.copy(p_data);
	m_config_changed = true;
}

void dsp_manager::dsp_run(t_dsp_chain::const_iterator p_iter,dsp_chunk_list * p_list,const metadb_handle_ptr & cur_file,unsigned flags,double & latency,abort_callback & p_abort)
{
	p_list->remove_bad_chunks();

	TRACK_CODE("dsp::run",p_iter->m_dsp->run_abortable(p_list,cur_file,flags,p_abort));
	TRACK_CODE("dsp::get_latency",latency += p_iter->m_dsp->get_latency());
}

double dsp_manager::run(dsp_chunk_list * p_list,const metadb_handle_ptr & p_cur_file,unsigned p_flags,abort_callback & p_abort) {
	TRACK_CALL_TEXT("dsp_manager::run");

	try {
		fpu_control_default l_fpu_control;

		double latency=0;
		bool done = false;

		t_dsp_chain::const_iterator flush_mark;
		if ((p_flags & dsp::END_OF_TRACK) && ! (p_flags & dsp::FLUSH)) {
			for(t_dsp_chain::const_iterator iter = m_chain.first(); iter.is_valid(); ++iter) {
				if (iter->m_dsp->need_track_change_mark()) flush_mark = iter;
			}
		}

		if (m_config_changed)
		{
			t_dsp_chain newchain;
			bool recycle_available = true;

			for(t_size n=0;n<m_config.get_count();n++) {
				service_ptr_t<dsp> temp;

				const dsp_preset & preset = m_config.get_item(n);
				if (dsp_entry::g_dsp_exists(preset.get_owner())) {
					t_dsp_chain::iterator iter = newchain.insert_last();
					iter->m_preset = m_config.get_item(n);
					iter->m_recycle_flag = false;
				}
			}


			//HACK: recycle existing DSPs in a special case when user has apparently only altered settings of one of DSPs.
			if (newchain.get_count() == m_chain.get_count()) {
				t_size data_mismatch_count = 0;
				t_size owner_mismatch_count = 0;
				t_dsp_chain::iterator iter_src, iter_dst;
				iter_src = m_chain.first(); iter_dst = newchain.first();
				while(iter_src.is_valid() && iter_dst.is_valid()) {
					if (iter_src->m_preset.get_owner() != iter_dst->m_preset.get_owner()) {
						owner_mismatch_count++;
					} else if (iter_src->m_preset != iter_dst->m_preset) {
						data_mismatch_count++;
					}
					++iter_src; ++iter_dst;
				}
				recycle_available = (owner_mismatch_count == 0 && data_mismatch_count <= 1);
			} else {
				recycle_available = false;
			}

			if (recycle_available) {
				t_dsp_chain::iterator iter_src, iter_dst;
				iter_src = m_chain.first(); iter_dst = newchain.first();
				while(iter_src.is_valid() && iter_dst.is_valid()) {
					if (iter_src->m_preset == iter_dst->m_preset) {
						iter_src->m_recycle_flag = true;
						iter_dst->m_dsp = iter_src->m_dsp;
					}
					++iter_src; ++iter_dst;
				}
			}

			for(t_dsp_chain::iterator iter = newchain.first(); iter.is_valid(); ++iter) {
				if (iter->m_dsp.is_empty()) {
					if (!dsp_entry::g_instantiate(iter->m_dsp,iter->m_preset)) throw pfc::exception_bug_check_v2();
				}
			}

			if (m_chain.get_count()>0) {
				bool flushflag = flush_mark.is_valid();
				for(t_dsp_chain::const_iterator iter = m_chain.first(); iter.is_valid(); ++iter) {
					unsigned flags2 = p_flags;
					if (iter == flush_mark) flushflag = false;
					if (flushflag || !iter->m_recycle_flag) flags2|=dsp::FLUSH;
					dsp_run(iter,p_list,p_cur_file,flags2,latency,p_abort);
				}
				done = true;
			}

			m_chain = newchain;
			m_config_changed = false;
		}
		
		if (!done)
		{
			bool flushflag = flush_mark.is_valid();
			for(t_dsp_chain::const_iterator iter = m_chain.first(); iter.is_valid(); ++iter) {
				unsigned flags2 = p_flags;
				if (iter == flush_mark) flushflag = false;
				if (flushflag) flags2|=dsp::FLUSH;
				dsp_run(iter,p_list,p_cur_file,flags2,latency,p_abort);
			}
			done = true;
		}
		
		p_list->remove_bad_chunks();

		return latency;
	} catch(...) {
		p_list->remove_all();
		throw;
	}
}

void dsp_manager::flush()
{
	for(t_dsp_chain::const_iterator iter = m_chain.first(); iter.is_valid(); ++iter) {
		TRACK_CODE("dsp::flush",iter->m_dsp->flush());
	}
}


bool dsp_manager::is_active() const {return m_config.get_count()>0;}

void dsp_config_manager::core_enable_dsp(const dsp_preset & preset) {
	dsp_chain_config_impl cfg;
	get_core_settings(cfg);

	bool found = false;
	bool changed = false;
	t_size n,m = cfg.get_count();
	for(n=0;n<m;n++) {
		if (cfg.get_item(n).get_owner() == preset.get_owner()) {
			found = true;
			if (cfg.get_item(n) != preset) {
				cfg.replace_item(preset,n);
				changed = true;
			}
			break;
		}
	}
	if (!found) {cfg.insert_item(preset,0); changed = true;}

	if (changed) set_core_settings(cfg);
}
void dsp_config_manager::core_disable_dsp(const GUID & id) {
	dsp_chain_config_impl cfg;
	get_core_settings(cfg);

	t_size n,m = cfg.get_count();
	bit_array_bittable mask(m);
	bool changed = false;
	for(n=0;n<m;n++) {
		bool axe = (cfg.get_item(n).get_owner() == id) ? true : false;
		if (axe) changed = true;
		mask.set(n,axe);
	}
	if (changed) {
		cfg.remove_mask(mask);
		set_core_settings(cfg);
	}
}

bool dsp_config_manager::core_query_dsp(const GUID & id, dsp_preset & out) {
	dsp_chain_config_impl cfg;
	get_core_settings(cfg);
	for(t_size n=0;n<cfg.get_count();n++) {
		const dsp_preset & entry = cfg.get_item(n);
		if (entry.get_owner() == id) {
			out = entry; return true;
		}
	}
	return false;
}
