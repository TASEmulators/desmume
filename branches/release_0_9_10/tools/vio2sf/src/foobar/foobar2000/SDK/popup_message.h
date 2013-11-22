
//! This interface allows you to show generic nonmodal noninteractive dialog with a text message. This should be used instead of MessageBox where possible.\n
//! Usage: use popup_message::g_show / popup_message::g_show_ex static helpers, or static_api_ptr_t<popup_message>.\n
//! Note that all strings are UTF-8.

class NOVTABLE popup_message : public service_base {
public:
	enum t_icon {icon_information, icon_error, icon_query};
	//! Activates the popup dialog; returns immediately (the dialog remains visible).
	//! @param p_msg Message to show (UTF-8 encoded string).
	//! @param p_msg_length Length limit of message string to show, in bytes (actual string may be shorter if null terminator is encountered before). Set this to infinite to use plain null-terminated strings.
	//! @param p_title Title of dialog to show (UTF-8 encoded string).
	//! @param p_title_length Length limit of the title string, in bytes (actual string may be shorter if null terminator is encountered before). Set this to infinite to use plain null-terminated strings.
	//! @param p_icon Icon of the dialog - can be set to icon_information, icon_error or icon_query.
	virtual void show_ex(const char * p_msg,unsigned p_msg_length,const char * p_title,unsigned p_title_length,t_icon p_icon = icon_information) = 0;

	//! Activates the popup dialog; returns immediately (the dialog remains visible); helper function built around show_ex(), takes null terminated strings with no length limit parameters.
	//! @param p_msg Message to show (UTF-8 encoded string).
	//! @param p_title Title of dialog to show (UTF-8 encoded string).
	//! @param p_icon Icon of the dialog - can be set to icon_information, icon_error or icon_query.
	inline void show(const char * p_msg,const char * p_title,t_icon p_icon = icon_information) {show_ex(p_msg,infinite,p_title,infinite,p_icon);}

	//! Static helper function instantiating the service and activating the message dialog. See show_ex() for description of parameters.
	static void g_show_ex(const char * p_msg,unsigned p_msg_length,const char * p_title,unsigned p_title_length,t_icon p_icon = icon_information);
	//! Static helper function instantiating the service and activating the message dialog. See show() for description of parameters.
	static inline void g_show(const char * p_msg,const char * p_title,t_icon p_icon = icon_information) {g_show_ex(p_msg,infinite,p_title,infinite,p_icon);}

	static void g_complain(const char * what) {
		g_show(what, "Information", icon_error);
	}

	static void g_complain(const char * p_whatFailed, const std::exception & p_exception) {
		g_complain(p_whatFailed,p_exception.what());
	}
	static void g_complain(const char * p_whatFailed, const char * msg) {
		g_complain(pfc::string_formatter() << p_whatFailed << ": " << msg);
	}

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(popup_message);
};

#define EXCEPTION_TO_POPUP_MESSAGE(CODE,LABEL) try { CODE; } catch(std::exception const & e) {popup_message::g_complain(LABEL,e);}
