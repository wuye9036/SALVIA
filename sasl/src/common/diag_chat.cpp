#include <sasl/include/common/diag_chat.h>

#include <sasl/include/common/diag_item.h>
#include <eflib/include/diagnostics/assert.h>

using eflib::fixed_string;
using std::unique_ptr;
using std::shared_ptr;
using std::vector;

BEGIN_NS_SASL_COMMON();

diag_item_committer* diag_item_committer::p(char const* v)
{
	(*item) % std::string(v);
	return this;
}

shared_ptr<diag_chat> diag_chat::create()
{
	return shared_ptr<diag_chat>( new diag_chat() );
}

void diag_chat::add_report_raised_handler( report_handler_fn const& handler )
{
	handlers.push_back( handler );
}

diag_chat::committer_pointer diag_chat::report( diag_template const& tmpl )
{
	diag_item* diag = new diag_item(&tmpl);
	diag_item_committer* ret( new diag_item_committer(diag, this) );
	return committer_pointer(ret);
}

diag_chat* diag_chat::merge( diag_chat* dest, diag_chat* src, bool trigger_callback )
{
	dest->diags.insert( dest->diags.end(), src->diags.begin(), src->diags.end() );
	
	if( trigger_callback )
	{
		for( vector<diag_item*>::iterator diag_it = src->diags.begin(); diag_it != src->diags.end(); ++diag_it )
		{
			for( vector<report_handler_fn>::iterator handler_it = dest->handlers.begin(); handler_it != dest->handlers.end(); ++handler_it )
			{
				(*handler_it)( dest, (*diag_it) );
			}
		}
	}

	src->diags.clear();
	return dest; 
}

void diag_chat::restore()
{
	for(size_t i_diag = cursors.back(); i_diag < diags.size(); ++i_diag)
	{
		diags[i_diag]->release();
	}
	diags.resize( cursors.back() );
	cursors.pop_back();
}

void diag_chat::save()
{
	cursors.push_back( diags.size() );
}

diag_chat::~diag_chat()
{
	for( vector<diag_item*>::iterator it = diags.begin(); it != diags.end(); ++it )
	{
		(*it)->release();
	}
}

void diag_chat::commit( diag_item* diag )
{
	diags.push_back( diag );
	for( vector<report_handler_fn>::iterator it = handlers.begin(); it != handlers.end(); ++it )
	{
		(*it)( this, diag );
	}
}

vector<diag_item*> const& diag_chat::diag_items() const
{
	return diags;
}

void diag_chat::clear()
{
	for( vector<diag_item*>::iterator it = diags.begin(); it != diags.end(); ++it )
	{
		(*it)->release();
	}
	diags.clear();
}


diag_item_committer::diag_item_committer( diag_item* item, diag_chat* chat )
	: item(item), chat(chat)
{
}

diag_item_committer* diag_item_committer::file(fixed_string const& f)
{
	item->file(f);
	return this;
}

diag_item_committer* diag_item_committer::span( code_span const& s )
{
	item->span(s);
	return this;
}

diag_item_committer::~diag_item_committer()
{
	chat->commit(item);
}

diag_item_committer* diag_item_committer::eval()
{
	item->eval();
	return this;
}

diag_item_committer* diag_item_committer::token_range( token_t const& beg, token_t const& end )
{
	item->file( beg.file_name );
	item->span( beg, end );
	return this;
}

size_t error_count( diag_chat* chat, bool warning_as_error )
{
	size_t count = 0;

	for( size_t i_diag = 0; i_diag < chat->diag_items().size(); ++i_diag )
	{
		diag_item* diag = chat->diag_items()[i_diag];
		if( diag->level() == dl_error || diag->level() == dl_fatal_error )
		{
			++count;
		} 
		else if( warning_as_error && diag->level() == dl_warning )
		{
			++count;
		}
	}

	return count;
}

END_NS_SASL_COMMON();