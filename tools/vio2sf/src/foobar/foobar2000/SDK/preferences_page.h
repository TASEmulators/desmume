#ifndef _FOOBAR2000_SDK_PREFERENCES_PAGE_H_
#define _FOOBAR2000_SDK_PREFERENCES_PAGE_H_

//! Implementing this service will generate a page in preferences dialog. Use preferences_page_factory_t template to register.
class NOVTABLE preferences_page : public service_base {
public:
	//! Creates preferences page dialog window. It is safe to assume that two dialog instances will never coexist. Caller is responsible for embedding it into preferences dialog itself.
	virtual HWND create(HWND p_parent) = 0;
	//! Retrieves name of the prefernces page to be displayed in preferences tree (static string).
	virtual const char * get_name() = 0;
	//! Retrieves GUID of the page.
	virtual GUID get_guid() = 0;
	//! Retrieves GUID of parent page/branch of this page. See preferences_page::guid_* constants for list of standard parent GUIDs. Can also be a GUID of another page or a branch (see: preferences_branch).
	virtual GUID get_parent_guid() = 0;
	//! Queries whether this page supports "reset page" feature.
	virtual bool reset_query() = 0;
	//! Activates "reset page" feature. It is safe to assume that the preferences page dialog does not exist at the point this is called (caller destroys it before calling reset and creates it again afterwards).
	virtual void reset() = 0;
	//! Retrieves help URL. Without overriding it, it will redirect to foobar2000 wiki.
	virtual bool get_help_url(pfc::string_base & p_out);

	static void get_help_url_helper(pfc::string_base & out, const char * category, const GUID & id, const char * name);
	
	static const GUID guid_root, guid_hidden, guid_tools,guid_core,guid_display,guid_playback,guid_visualisations,guid_input,guid_tag_writing,guid_media_library, guid_tagging;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(preferences_page);
};

class NOVTABLE preferences_page_v2 : public preferences_page {
public:
	//! Allows custom sorting order of preferences pages. Return lower value for higher priority (lower resulting index in the list). When sorting priority of two items matches, alphabetic sorting is used. Return 0 to use default alphabetic sorting without overriding priority.
	virtual double get_sort_priority() = 0;

	FB2K_MAKE_SERVICE_INTERFACE(preferences_page_v2,preferences_page);
};

template<class T>
class preferences_page_factory_t : public service_factory_single_t<T> {};

//! Creates a preferences branch - an empty page that only serves as a parent for other pages and is hidden when no child pages exist. Instead of implementing this, simply use preferences_branch_factory class to declare a preferences branch with specified parameters.
class NOVTABLE preferences_branch : public service_base {
public:
	//! Retrieves name of the preferences branch.
	virtual const char * get_name() = 0;
	//! Retrieves GUID of the preferences branch. Use this GUID as parent GUID for pages/branches nested in this branch.
	virtual GUID get_guid() = 0;
	//! Retrieves GUID of parent page/branch of this branch. See preferences_page::guid_* constants for list of standard parent GUIDs. Can also be a GUID of another branch or a page.
	virtual GUID get_parent_guid() = 0;
	

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(preferences_branch);
};

class preferences_branch_v2 : public preferences_branch {
public:
	//! Allows custom sorting order of preferences pages. Return lower value for higher priority (lower resulting index in the list). When sorting priority of two items matches, alphabetic sorting is used. Return 0 to use default alphabetic sorting without overriding priority.
	virtual double get_sort_priority() = 0;

	FB2K_MAKE_SERVICE_INTERFACE(preferences_branch_v2,preferences_branch);
};

class preferences_branch_impl : public preferences_branch_v2 {
public:
	preferences_branch_impl(const GUID & p_guid,const GUID & p_parent,const char * p_name,double p_sort_priority = 0) : m_guid(p_guid), m_parent(p_parent), m_name(p_name), m_sort_priority(p_sort_priority) {}
	const char * get_name() {return m_name;}
	GUID get_guid() {return m_guid;}
	GUID get_parent_guid() {return m_parent;}
	double get_sort_priority() {return m_sort_priority;}
private:
	const GUID m_guid,m_parent;
	const pfc::string8 m_name;
	const double m_sort_priority;
};

typedef service_factory_single_t<preferences_branch_impl> __preferences_branch_factory;

//! Instantiating this class declares a preferences branch with specified parameters.\n
//! Usage: static preferences_branch_factory g_mybranch(mybranchguid,parentbranchguid,"name of my preferences branch goes here");
class preferences_branch_factory : public __preferences_branch_factory {
public:
	preferences_branch_factory(const GUID & p_guid,const GUID & p_parent,const char * p_name,double p_sort_priority = 0) : __preferences_branch_factory(p_guid,p_parent,p_name,p_sort_priority) {}
};

#endif //_FOOBAR2000_SDK_PREFERENCES_PAGE_H_
