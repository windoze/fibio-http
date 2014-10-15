//
//  routing.cpp
//  fibio-http
//
//  Created by Chen Xu on 14/10/12.
//  Copyright (c) 2014 0d0a.com. All rights reserved.
//

#include <fibio/http/server/routing.hpp>
#include "url_parser.hpp"

namespace fibio { namespace http {
    match_type operator&&(const match_type &lhs, const match_type &rhs) {
        struct and_matcher {
            bool operator()(server::request &req, match_info &p) {
                return lhs_(req, p) && rhs_(req, p);
            }
            match_type lhs_;
            match_type rhs_;
        };
        
        return and_matcher{lhs, rhs};
    }
    
    match_type operator||(const match_type &lhs, const match_type &rhs) {
        struct or_matcher {
            bool operator()(server::request &req, match_info &p) {
                return lhs_(req, p) || rhs_(req,p);
            }
            match_type lhs_;
            match_type rhs_;
        };
        
        return or_matcher{lhs, rhs};
    }
    
    match_type operator!(const match_type &m) {
        struct not_matcher {
            bool operator()(server::request &req, match_info &p) { return !op_(req, p); }
            match_type op_;
        };
        
       return not_matcher{m};
    }
    
    server::request_handler_type routing_table(const routing_table_type &table,
                                               server::request_handler_type default_handler)
    {
        struct routing_table_handler {
            bool operator()(server::request &req,
                            server::response &resp,
                            server::connection &conn)
            {
                parse_url(req.url, req.parsed_url);
                match_info p;
                for(auto &e : routing_table_) {
                    if(e.first(req, p)) {
                        return e.second(p, req, resp, conn);
                    } else {
                        // Clear parameters, in case some matcher accidentally wrote someting
                        p.clear();
                    }
                }
                return default_handler_(req, resp, conn);
            }
            
            routing_table_type routing_table_;
            server::request_handler_type default_handler_;
        };
        
        return routing_table_handler{table, default_handler};
    }

    match_type match_any() {
        struct any_matcher {
            bool operator()(server::request &, match_info &) const
            { return true; }
        };
        return any_matcher();
    }
    
    const match_type any;

    match_type method_is(http_method m) {
        struct method_matcher {
            bool operator()(server::request &req, match_info &) const {
                return req.method==method_;
            }
            http_method method_;
        };
        return method_matcher{m};
    }
    
    match_type version_is(http_version v) {
        struct version_matcher {
            bool operator()(server::request &req, match_info &) const {
                return req.version==version_;
            }
            http_version version_;
        };
        return version_matcher{v};
    }
    
    match_type path_match(const std::string &tmpl) {
        struct path_matcher {
            typedef std::list<std::string> components_type;
            typedef components_type::const_iterator component_iterator;
            
            bool operator()(server::request &req, match_info &m) {
                parse_url(req.url, req.parsed_url);
                component_iterator p=pattern.cbegin();
                for (auto &i : req.parsed_url.path_components) {
                    if (i.empty()) {
                        // Skip empty component
                        continue;
                    }
                    if (p==pattern.cend()) {
                        // End of pattern
                        return false;
                    } else if ((*p)[0]==':') {
                        // This pattern component is a parameter
                        m.insert({std::string(p->begin()+1, p->end()), i});
                    } else if ((*p)[0]=='*') {
                        if (p->length()==1) {
                            // Ignore anything remains if the wildcard doesn't have a name
                            return true;
                        }
                        std::string param_name(p->begin()+1, p->end());
                        auto mi=m.find(param_name);
                        if (mi==m.end()) {
                            // Not found
                            m.insert({param_name, i});
                        } else {
                            // Concat this component to existing parameter
                            mi->second.push_back('/');
                            mi->second.append(i);
                        }
                        // NOTE: Do not increment p
                        continue;
                    } else if (*p!=i) {
                        // Not match
                        return false;
                    }
                    ++p;
                }
                // Either pattern consumed or ended with a wildcard
                return p==pattern.end() || (*p)[0]=='*';
            }
            std::list<std::string> pattern;
            common::iequal eq;
        };
        path_matcher m;
        std::vector<std::string> c;
        common::parse_path_components(tmpl, m.pattern);
        return std::move(m);
    }
    
    match_type rest_resources(const std::string &path) {
        // Collection operations
        match_type a=(method_is(http_method::GET)
                      || method_is(http_method::PUT)
                      || method_is(http_method::POST)
                      || method_is(http_method::DELETE)) && path_match(path);
        // Item operations
        match_type b=(method_is(http_method::GET)
                      || method_is(http_method::PUT)
                      || method_is(http_method::DELETE)) && path_match(path+"/:id");
        return a || b;
    }
}}  // End of namespace fibio::http
