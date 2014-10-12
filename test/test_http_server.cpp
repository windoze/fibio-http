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
#include <fibio/fiber.hpp>
#include <fibio/http/client/client.hpp>
#include <fibio/http/server/server.hpp>
#include <fibio/fiberize.hpp>

using namespace fibio;
using namespace fibio::http;

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

bool handler(server::request &req, server::response &resp, server::connection &c) {
    // Check request
    assert(req.content_length==strlen("hello"));
    std::stringstream ss;
    ss << req.body_stream().rdbuf();
    assert(ss.str()==std::string("hello"));
    
    // Write all headers back in a table
    resp.set_content_type("text/html");
    resp.body_stream() << "<HTML><HEAD><TITLE>Test</TITLE></HEAD><BODY><TABLE>" << std::endl;
    for(auto &p: req.headers) {
        resp.body_stream() << "<TR><TD>" << p.first << "</TD><TD>" << p.second << "</TD></TR>" <<std::endl;
    }
    resp.body_stream() << "</TABLE></BODY></HTML>" << std::endl;
    return true;
}

int fibio::main(int argc, char *argv[]) {
    //scheduler::get_instance().add_worker_thread(3);

    server svr(server::settings{"127.0.0.1", 23456, handler});
    svr.start();
    {
        // Create some clients, do some requests
        fiber_group fibers;
        size_t n=10;
        for (int i=0; i<n; i++) {
            fibers.create_fiber(the_client);
        }
        fibers.join_all();
    }
    svr.stop();
    svr.join();
    std::cout << "main_fiber exiting" << std::endl;
    return 0;
}
