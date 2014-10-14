//
//  routing.hpp
//  fibio-http
//
//  Created by Chen Xu on 14/10/12.
//  Copyright (c) 2014 0d0a.com. All rights reserved.
//

#ifndef fibio_http_server_routing_hpp
#define fibio_http_server_routing_hpp

#include <list>
#include <functional>
#include <boost/optional.hpp>
#include <fibio/http/server/server.hpp>

namespace fibio { namespace http {
    /**
     * Record matched parts
     */
    typedef std::map<std::string, std::string> match_info;
    
    /**
     * Check if request meets specific criteria
     */
    typedef std::function<bool(server::request &, match_info &)> match_type;
    
    /**
     * Match functor operators
     */
    match_type operator&&(const match_type &lhs, const match_type &rhs);
    match_type operator||(const match_type &lhs, const match_type &rhs);
    match_type operator!(const match_type &m);

    /**
     * Routing table
     */
    typedef std::function<bool(match_info &,
                               server::request &,
                               server::response &,
                               server::connection &)> routing_handler_type;
    typedef std::pair<match_type, routing_handler_type> routing_rule_type;
    typedef std::list<routing_rule_type> routing_table_type;
    
    /**
     * Fallback handler, close connection unconditionally
     */
    extern const server::request_handler_type fallback_handler;
    
    server::request_handler_type routing_table(const routing_table_type &table,
                                               server::request_handler_type default_handler=fallback_handler);

    /**
     * Match any request
     */
    extern const match_type any;

    /**
     * Match HTTP method
     */
    match_type method_is(http_method m);
    
    /**
     * Match HTTP version
     */
    match_type version_is(http_version v);
    
    // Check a string attribute, like URL or header
    typedef std::function<bool(const std::string &, match_info &)> string_match_type;
    match_type url_(const string_match_type &);
    match_type header_(const std::string &header, const string_match_type &);

    // Match path pattern and extract parameters into match_info
    match_type path_match(const std::string &tmpl);
    
    // Match RESTful requests, may generate "id" parameter in match_info
    match_type rest_resources(const std::string &path);
}}  // End of namespace fibio::http

#endif
