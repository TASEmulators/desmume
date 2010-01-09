class NOVTABLE event_logger : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE(event_logger, service_base);
public:
	enum {
		severity_status,
		severity_warning,
		severity_error
	};
	void log_status(const char * line) {log_entry(line, severity_status);}
	void log_warning(const char * line) {log_entry(line, severity_warning);}
	void log_error(const char * line) {log_entry(line, severity_error);}
	
	virtual void log_entry(const char * line, unsigned severity) = 0;
};

class event_logger_fallback : public event_logger {
public:
	void log_entry(const char * line, unsigned) {console::print(line);}
};
