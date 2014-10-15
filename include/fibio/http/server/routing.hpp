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
#include <fibio/http/common/string_pred.hpp>

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
    typedef std::list<std::pair<match_type, routing_handler_type>> routing_table_type;
 
    /**
     * Stock response with specific status code, can be used with http server or routing table
     */
    struct stock_handler{
        operator server::request_handler_type() const {
            return [=](server::request &,
                      server::response &resp,
                      server::connection &) -> bool {
                resp.status_code=m;
                return true;
            };
        }
        
        operator routing_handler_type() const {
            return [=](match_info &,
                       server::request &,
                       server::response &resp,
                       server::connection &) -> bool {
                resp.status_code=m;
                return true;
            };
        }
        
        http_status_code m;
    };

    /**
     * Routing table to handle requests
     */
    server::request_handler_type routing_table(const routing_table_type &table,
                                               server::request_handler_type default_handler=stock_handler{http_status_code::NOT_FOUND});

    
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
    
    /**
     * Check URL against pred
     * !see http/common/string_pred.hpp
     */
    template<typename Predicate>
    match_type url_(Predicate pred) {
        return [pred](server::request &req, match_info &)->bool {
            return pred(req.url);
        };
    }

    /**
     * Check specific header against pred
     * !see http/common/string_pred.hpp
     */
    template<typename Predicate>
    match_type header_(const std::string &h, Predicate pred) {
        return [h, pred](server::request &req, match_info &)->bool {
            auto i=req.headers.find(h);
            if (i==req.headers.end()) {
                return false;
            }
            return pred(i->second);
        };
    }
    
    // Match path pattern and extract parameters into match_info
    match_type path_match(const std::string &tmpl);
    
    // Match RESTful requests, may generate "id" parameter in match_info
    match_type rest_resources(const std::string &path);
}}  // End of namespace fibio::http

#endif
