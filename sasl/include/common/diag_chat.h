#ifndef SASL_COMMON_DIAG_CHAT_H
#define SASL_COMMON_DIAG_CHAT_H

#include <sasl/include/common/common_fwd.h>

#include <eflib/include/string/ustring.h>

#include <memory>
#include <functional>
#include <vector>

BEGIN_NS_SASL_COMMON();

struct	token_t;
class	diag_chat;
class	diag_item;
class	diag_template;
struct	code_span;

typedef std::function<bool (diag_chat*, diag_item*)> report_handler_fn;

class diag_item_committer
{
public:
	friend class diag_chat;
	~diag_item_committer();

	template <typename T>
	diag_item_committer* p( T const& v )
	{
		(*item) % v;
		return this;
	}

	diag_item_committer*	p(char const* v);

	diag_item_committer*	eval();
	diag_item_committer*	token_range(token_t const& beg, token_t const& end);
	diag_item_committer*	file(eflib::fixed_string const& f);
	diag_item_committer*	span(code_span const& s);

private:
	diag_item_committer( diag_item* item, diag_chat* chat );

	diag_item_committer( diag_item_committer const& );
	diag_item_committer& operator = ( diag_item_committer const& );

	diag_item*	item;
	diag_chat*	chat;
};

class diag_chat
{
public:
	typedef std::unique_ptr<diag_item_committer> committer_pointer;

	friend class diag_item_committer;
	static std::shared_ptr<diag_chat> create();
	static diag_chat* merge( diag_chat* dest, diag_chat* src, bool trigger_callback );

	void add_report_raised_handler( report_handler_fn const& handler );
	committer_pointer report( diag_template const& tmpl );
	
	std::vector<diag_item*> const& diag_items() const;
	void clear();

	void save();
	void restore();

	~diag_chat();
private:
	void commit(diag_item* diag);
	std::vector<report_handler_fn>	handlers;
	std::vector<diag_item*>			diags;
	std::vector<size_t>				cursors;
};

class chat_scope
{
public:
	chat_scope(diag_chat* chat): chat(chat), need_save(false) { chat->save(); }
	~chat_scope() { if(!need_save) chat->restore(); }
	void save_new_diags(){ need_save = true; }
private:
	bool		need_save;
	diag_chat*	chat;
};
// Functions

size_t error_count( diag_chat* chat, bool warning_as_error );

END_NS_SASL_COMMON();

#endif