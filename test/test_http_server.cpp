//
//  test_http_client.cpp
//  fibio
//
//  Created by Chen Xu on 14-3-12.
//  Copyright (c) 2014 0d0a.com. All rights reserved.
//

#include <iostream>
#include <vector>
#include <chrono>
#include <sstream>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <fibio/fiber.hpp>
#include <fibio/fiberize.hpp>
#include <fibio/http/client/client.hpp>
#include <fibio/http/server/server.hpp>
#include <fibio/http/server/routing.hpp>

using namespace fibio;
using namespace fibio::http;
using namespace fibio::http::common;

void the_client() {
    client c;
    client::request req;
    req.url="/";
    // Default method
    assert(req.method==http_method::INVALID);
    req.method=http_method::GET;
    // Default version
    assert(req.version==http_version::INVALID);
    req.version=http_version::HTTP_1_1;
    assert(req.get_content_length()==0);
    req.keep_alive=true;
    std::string req_body("hello");
    req.body_stream() << req_body;
    
    if(c.connect("127.0.0.1", 23456)) {
        assert(false);
    }
    
    for(int i=0; i<10; i++) {
        client::response resp;
        if(c.send_request(req, resp)) {
            // This server returns a 200 response
            assert(resp.status_code==http_status_code::OK);
            assert(resp.status_message=="OK");
            assert(resp.version==http_version::HTTP_1_1);
            
            size_t cl=resp.content_length;
            std::string s;
            std::stringstream ss;
            ss << resp.body_stream().rdbuf();
            s=ss.str();
            assert(s.size()==cl);
            // Make sure we triggered eof
            resp.body_stream().peek();
            assert(resp.body_stream().eof());
        } else {
            assert(false);
        }
    }
}

struct path_is {
    bool operator()(const std::string &s, match_info &p) const {
        return s==path_;
    }
    std::string path_;
};

bool handler(match_info &mi, server::request &req, server::response &resp, server::connection &c) {
    // Write all headers back in a table
    resp.set_content_type("text/html");
    resp.body_stream() << "<HTML><HEAD><TITLE>Test</TITLE></HEAD><BODY>"<< std::endl;
    resp.body_stream() << "<H1>Request Info</H1>" << std::endl;
    resp.body_stream() << "<TABLE>" << std::endl;
    resp.body_stream() << "<TR><TD>URL</TD><TD>" << req.url << "</TD></TR>" << std::endl;
    resp.body_stream() << "<TR><TD>Schema</TD><TD>" << req.parsed_url.schema << "</TD></TR>" << std::endl;
    resp.body_stream() << "<TR><TD>Port</TD><TD>" << req.parsed_url.port << "</TD></TR>" << std::endl;
    resp.body_stream() << "<TR><TD>Path</TD><TD>" << req.parsed_url.path << "</TD></TR>" << std::endl;
    resp.body_stream() << "<TR><TD>Query</TD><TD>" << req.parsed_url.query << "</TD></TR>" << std::endl;
    resp.body_stream() << "<TR><TD>User Info</TD><TD>" << req.parsed_url.userinfo << "</TD></TR>" << std::endl;
    resp.body_stream() << "</TABLE>" << std::endl;
    resp.body_stream() << "<H1>Headers</H1>" << std::endl;
    resp.body_stream() << "<TABLE>" << std::endl;
    for(auto &p: req.headers) {
        resp.body_stream() << "<TR><TD>" << p.first << "</TD><TD>" << p.second << "</TD></TR>" <<std::endl;
    }
    resp.body_stream() << "</TABLE>" << std::endl;
    resp.body_stream() << "<H1>Parameters</H1>" << std::endl;
    resp.body_stream() << "<TABLE>" << std::endl;
    for(auto &p: mi) {
        resp.body_stream() << "<TR><TD>" << p.first << "</TD><TD>" << p.second << "</TD></TR>" <<std::endl;
    }
    resp.body_stream() << "</TABLE>" << std::endl;
    resp.body_stream() << "<H1>Query</H1>" << std::endl;
    resp.body_stream() << "<TABLE>" << std::endl;
    for(auto &p: req.parsed_url.query_params) {
        resp.body_stream() << "<TR><TD>" << p.first << "</TD><TD>" << p.second << "</TD></TR>" <<std::endl;
    }
    resp.body_stream() << "</TABLE>" << std::endl;
    resp.body_stream() << "</BODY></HTML>" << std::endl;
    return true;
}

bool handler_404(server::request &req, server::response &resp, server::connection &c) {
    resp.status_code=http_status_code::NOT_FOUND;
    resp.keep_alive=false;
    return true;
}

bool handler_400(match_info &, server::request &req, server::response &resp, server::connection &c) {
    resp.status_code=http_status_code::BAD_REQUEST;
    resp.keep_alive=false;
    return true;
}

int fibio::main(int argc, char *argv[]) {
    scheduler::get_instance().add_worker_thread(3);
    
    server svr(server::settings{"127.0.0.1",
        23456,
        routing_table(routing_table_type{
            {url_(starts_with{"/"}), handler},
            {path_match("/")
                || path_match("/index.html")
                || path_match("/index.htm"), handler},
            {path_match("/test1/:id/test2"), handler},
            {path_match("/test2/*p") && method_is(http_method::POST), handler},
            {path_match("/test3/*"), handler},
            {!method_is(http_method::GET), stock_handler{http_status_code::BAD_REQUEST}}
        }, stock_handler{http_status_code::NOT_FOUND})
    });
    svr.start();
    {
        // Create some clients, do some requests
        fiber_group fibers;
        size_t n=10;
        for (int i=0; i<n; i++) {
            //fibers.create_fiber(the_client);
        }
        fibers.join_all();
    }
    //svr.stop();
    svr.join();
    std::cout << "main_fiber exiting" << std::endl;
    return 0;
}
