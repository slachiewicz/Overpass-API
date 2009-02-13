#include <cctype>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <stdlib.h>
#include <vector>
#include "expat_justparse_interface.h"
#include "script_datatypes.h"
#include "script_queries.h"
#include "script_tools.h"
#include "user_interface.h"
#include "vigilance_control.h"

#include <mysql.h>

using namespace std;

const char* types_lowercase[] = { "", "node", "way", "relation", "area" };
const char* types_uppercase[] = { "", "Node", "Way", "Relation", "Area" };

const char* NODE_DATA = "/opt/osm_why_api/nodes.dat";
const char* NODE_IDX = "/opt/osm_why_api/nodes.b.idx";
const char* NODE_IDXA = "/opt/osm_why_api/nodes.1.idx";

const char* WAY_DATA = "/opt/osm_why_api/ways.dat";
const char* WAY_IDX = "/opt/osm_why_api/ways.b.idx";
const char* WAY_IDXA = "/opt/osm_why_api/ways.1.idx";
const char* WAY_IDXSPAT = "/opt/osm_why_api/ways.spatial.idx";
const char* WAY_ALLTMP = "/tmp/ways.dat";

static string rule_name_;
static int rule_id_;
static vector< pair< int, int > > stack;

void set_rule(int rule_id, string rule_name)
{
  rule_id_ = rule_id;
  rule_name_ = rule_name;
}

int get_rule_id()
{
  return rule_id_;
}

string get_rule_name()
{
  return rule_name_;
}

void push_stack(int type, int id)
{
  stack.push_back(make_pair< int, int>(type, id));
}

void pop_stack()
{
  stack.pop_back();
}

const vector< pair< int, int > >& get_stack()
{
  return stack;
}

int next_stmt_id()
{
  static int next_stmt_id(0);
  return ++next_stmt_id;
}

//-----------------------------------------------------------------------------

void eval_cstr_array(string element, map< string, string >& attributes, const char **attr)
{
  for (unsigned int i(0); attr[i]; i += 2)
  {
    map< string, string >::iterator it(attributes.find(attr[i]));
    if (it != attributes.end())
      it->second = attr[i+1];
    else
    {
      ostringstream temp;
      temp<<"Unknown attribute \""<<attr[i]<<"\" in element \""<<element<<"\".";
      add_static_error(temp.str());
    }
  }
}

void substatement_error(string parent, Statement* child)
{
  ostringstream temp;
  temp<<"Element \""<<child->get_name()<<"\" cannot be subelement of element \""<<parent<<"\".";
  add_static_error(temp.str());
  
  delete child;
}

//-----------------------------------------------------------------------------

void assure_no_text(string text, string name)
{
  for (unsigned int i(0); i < text.size(); ++i)
  {
    if (!isspace(text[i]))
    {
      ostringstream temp;
      temp<<"Element \""<<name<<"\" must not contain text.";
      add_static_error(temp.str());
      break;
    }
  }
}

void Statement::add_statement(Statement* statement, string text)
{
  assure_no_text(text, this->get_name());
  substatement_error(get_name(), statement);
}

void Statement::add_final_text(string text)
{
  assure_no_text(text, this->get_name());
}

void Statement::display_full()
{
  display_verbatim(get_source(startpos, endpos - startpos), cout);
}

void Statement::display_starttag()
{
  display_verbatim(get_source(startpos, tagendpos - startpos), cout);
}

//-----------------------------------------------------------------------------

int script_timeout(0);
int element_limit(0);

void Root_Statement::set_attributes(const char **attr)
{
  map< string, string > attributes;
  
  attributes["timeout"] = "0";
  attributes["element-limit"] = "0";
  attributes["name"] = "";
  attributes["replace"] = "0";
  attributes["version"] = "0";
  attributes["debug"] = "quiet";
  
  eval_cstr_array(get_name(), attributes, attr);
  
  name = attributes["name"];
  replace = atoi(attributes["replace"].c_str());
  timeout = atoi(attributes["timeout"].c_str());
  elem_limit = atoi(attributes["element-limit"].c_str());
  version = atoi(attributes["version"].c_str());
  
  if (attributes["debug"] == "quiet")
    set_debug_mode(QUIET);
  if (attributes["debug"] == "errors")
    set_debug_mode(ERRORS);
  if (attributes["debug"] == "verbose")
    set_debug_mode(VERBOSE);
  if (attributes["debug"] == "static")
    set_debug_mode(STATIC_ANALYSIS);
  
  script_timeout = timeout;
  element_limit = elem_limit;
}

void Root_Statement::add_statement(Statement* statement, string text)
{
  assure_no_text(text, this->get_name());
  
  if ((statement->get_name() == "union") ||
       (statement->get_name() == "id-query") ||
       (statement->get_name() == "query") ||
       (statement->get_name() == "recurse") ||
       (statement->get_name() == "foreach") ||
       (statement->get_name() == "make-area") ||
       (statement->get_name() == "coord-query") ||
       (statement->get_name() == "bbox-query") ||
       (statement->get_name() == "print") ||
       (statement->get_name() == "conflict") ||
       (statement->get_name() == "report") ||
       (statement->get_name() == "detect-odd-nodes"))
    substatements.push_back(statement);
  else
    substatement_error(get_name(), statement);
}

void Root_Statement::forecast(MYSQL* mysql)
{
  for (vector< Statement* >::iterator it(substatements.begin());
       it != substatements.end(); ++it)
    (*it)->forecast(mysql);
}

void Root_Statement::execute(MYSQL* mysql, map< string, Set >& maps)
{
  if (timeout > 0)
  {
    if (add_timeout(mysql_thread_id(mysql), timeout))
      return;
  }
  
  for (vector< Statement* >::iterator it(substatements.begin());
       it != substatements.end(); ++it)
  {
    (*it)->execute(mysql, maps);
    
    statement_finished(*it);
  }

  if (timeout > 0)
    remove_timeout();
}

//-----------------------------------------------------------------------------

vector< string > role_cache;

const vector< string >& get_role_cache()
{
  return role_cache;
}

void prepare_caches(MYSQL* mysql)
{
  role_cache.push_back("");
  
  MYSQL_RES* result(mysql_query_wrapper(mysql, "select id, role from member_roles"));
  if (!result)
    return;
  
  MYSQL_ROW row(mysql_fetch_row(result));
  while ((row) && (row[0]) && (row[1]))
  {
    unsigned int id((unsigned int)atol(row[0]));
    if (role_cache.size() <= id)
      role_cache.resize(id+64);
    role_cache[id] = row[1];
    row = mysql_fetch_row(result);
  }
}

//-----------------------------------------------------------------------------

struct Flow_Forecast {
  int time_used_so_far;
  vector< pair< int, string > > pending_sets;
};

map< string, Set_Forecast > sets;

vector< Flow_Forecast > forecast_stack;

void declare_used_time(int milliseconds)
{
  forecast_stack.back().time_used_so_far += milliseconds;
}

const Set_Forecast& declare_read_set(string name)
{
  forecast_stack.back().pending_sets.push_back
	(make_pair< int, string >(READ_FORECAST, name));
  
  map< string, Set_Forecast >::const_iterator it(sets.find(name));
  if (it != sets.end())
    return it->second;
  else
  {
    ostringstream temp;
    temp<<"Statement reads from set \""<<name<<"\" which has not been used before.\n";
    add_sanity_remark(temp.str());
    return sets[name];
  }
}

Set_Forecast& declare_write_set(string name)
{
  forecast_stack.back().pending_sets.push_back
	(make_pair< int, string >(WRITE_FORECAST, name));
  
  sets[name] = Set_Forecast();
  return sets[name];
}

Set_Forecast& declare_union_set(string name)
{
  forecast_stack.back().pending_sets.push_back
	(make_pair< int, string >(UNION_FORECAST, name));
  
  return sets[name];
}

void inc_stack()
{
  int time_used_so_far(0);
  if (!(forecast_stack.empty()))
    time_used_so_far = forecast_stack.back().time_used_so_far;
  forecast_stack.push_back(Flow_Forecast());
  forecast_stack.back().time_used_so_far = time_used_so_far;
  if (forecast_stack.size() > 20)
    add_sanity_error("Stack exceeds size limit of 20.");
}

void dec_stack()
{
  int time_used_so_far(forecast_stack.back().time_used_so_far);
  forecast_stack.pop_back();
  if (!(forecast_stack.empty()))
    forecast_stack.back().time_used_so_far = time_used_so_far;
}

const vector< pair< int, string > >& pending_stack()
{
  return forecast_stack.back().pending_sets;
}

int stack_time_offset()
{
  if (forecast_stack.size() > 1)
    return (forecast_stack.back().time_used_so_far
	- forecast_stack[forecast_stack.size() - 2].time_used_so_far);
  else
    return forecast_stack.back().time_used_so_far;
}

void finish_statement_forecast()
{
  if ((script_timeout == 0) && (forecast_stack.back().time_used_so_far > 180*1000))
    add_sanity_error("Time exceeds limit of 180 seconds.");
  unsigned int element_count(0);
  for (map< string, Set_Forecast >::const_iterator it(sets.begin()); it != sets.end(); ++it)
  {
    element_count += it->second.node_count;
    element_count += 29*it->second.way_count;
    element_count += 26*it->second.relation_count;
  }
  if ((element_limit == 0) && (element_count > 10*1000*1000))
    add_sanity_error("Number of elements exceeds limit of 10,000,000 elements.");
}

void display_state()
{
  ostringstream temp;
  temp<<"The script will reach this point "
      <<((double)forecast_stack.back().time_used_so_far)/1000
      <<" seconds after start.<br/>\n";
  for (map< string, Set_Forecast >::const_iterator it(sets.begin()); ; )
  {
    temp<<"Set \""<<it->first<<"\" will contain here "
	<<it->second.node_count<<" nodes, "
	<<it->second.way_count<<" ways, "
	<<it->second.relation_count<<" relations and "
	<<it->second.area_count<<" areas.";
    if (++it != sets.end())
      temp<<"<br/>\n";
    else
      break;
  }
  display_state(temp.str(), cout);
}