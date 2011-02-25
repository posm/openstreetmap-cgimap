#include "nodes_handler.hpp"
#include "osm_helpers.hpp"
#include "http.hpp"
#include "logger.hpp"
#include "infix_ostream_iterator.hpp"

#include <sstream>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

namespace al = boost::algorithm;

using std::stringstream;
using std::list;
using std::vector;
using std::string;
using std::map;
using boost::format;
using boost::lexical_cast;
using boost::bad_lexical_cast;

nodes_responder::nodes_responder(list<id_t> ids_, pqxx::work &w_)
	: ids(ids_), w(w_) {

	stringstream query;
	list<id_t>::const_iterator it;
	
	query << "create temporary table tmp_nodes as select id from current_nodes where id IN (";
	std::copy(ids.begin(), ids.end(), infix_ostream_iterator<id_t>(query, ","));
	query << ") and visible";

	w.exec(query);
}

nodes_responder::~nodes_responder() throw() {
}

void 
nodes_responder::write(std::auto_ptr<output_formatter> f) {
	try {
		f->start_document();
		osm_helpers::write_tmp_nodes(w, *f, 1);
	} catch (const std::exception &e) {
		f->error(e);
	}
	f->end_document();
}

nodes_handler::nodes_handler(FCGX_Request &request) 
	: ids(validate_request(request)) {
}

nodes_handler::~nodes_handler() throw() {
}

std::string 
nodes_handler::log_name() const {
	return "nodes";
}

responder_ptr_t 
nodes_handler::responder(pqxx::work &x) const {
	return responder_ptr_t(new nodes_responder(ids, x));
}

formats::format_type 
nodes_handler::format() const {
	return formats::XML;
}

/**
 * Validates an FCGI request, returning the valid list of ids or 
 * throwing an error if there was no valid list of node ids.
 */
list<id_t>
nodes_handler::validate_request(FCGX_Request &request) {
	// check that the REQUEST_METHOD is a GET
	if (fcgi_get_env(request, "REQUEST_METHOD") != "GET") 
		throw http::method_not_allowed("Only the GET method is supported for "
									   "nodes requests.");

	string decoded = http::urldecode(get_query_string(request));
	const map<string, string> params = http::parse_params(decoded);
	map<string, string>::const_iterator itr = params.find("nodes");

	list <id_t> myids;

	if (itr != params.end()) {
		vector<string> strs;
		al::split(strs, itr->second, al::is_any_of(","));
		try {
			for (vector<string>::iterator itr = strs.begin(); itr != strs.end(); ++itr) { 
				id_t id = lexical_cast<id_t>(*itr);
				myids.push_back(id);
			}
		} catch (const bad_lexical_cast &) {
			throw http::bad_request("The parameter nodes is required, and must be "
									"of the form nodes=id[,id[,id...]].");
		}
	}
    
  return myids;
}