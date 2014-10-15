//
//  client.hpp
//  fibio
//
//  Created by Chen Xu on 14-3-11.
//  Copyright (c) 2014 0d0a.com. All rights reserved.
//

#ifndef fibio_http_client_client_hpp
#define fibio_http_client_client_hpp

#include <string>
#include <functional>
#include <fibio/stream/iostream.hpp>
#include <fibio/http/client/request.hpp>
#include <fibio/http/client/response.hpp>
#include <fibio/http/common/url_codec.hpp>

namespace fibio { namespace http {
    struct client {
        typedef fibio::http::client_request request;
        typedef fibio::http::client_response response;
        
        client()=default;
        client(const std::string &server, const std::string &port);
        client(const std::string &server, int port);
        
        boost::system::error_code connect(const std::string &server, const std::string &port);
        boost::system::error_code connect(const std::string &server, int port);
        void disconnect();
        
        void set_auto_decompress(bool c);
        bool get_auto_decompress() const;
        
        bool send_request(request &req, response &resp);
        
        std::string server_;
        std::string port_;
        stream::tcp_stream stream_;
        bool auto_decompress_=false;
    };
    
    // GET
    client::request &make_request(client::request &req,
                                  const std::string &url,
                                  const common::header_map &hdr=common::header_map());

    // POST
    template<typename T>
    client::request &make_request(client::request &req,
                      const std::string &url,
                      const T &body,
                      const common::header_map &hdr=common::header_map())
    {
        req.clear();
        req.url=url;
        req.method=http_method::POST;
        req.version=http_version::HTTP_1_1;
        req.keep_alive=true;
        req.headers.insert(hdr.begin(), hdr.end());
        // Default content type for HTML Forms
        req.set_content_type("application/x-www-form-urlencoded");
        // Write URL encoded body into body stream
        url_encode(body, std::ostreambuf_iterator<char>(req.body_stream()));
        return req;
    }
    
    struct url_client {
        client::response &request(const std::string &url,
                                  const common::header_map &hdr=common::header_map());
        
        template<typename T>
        client::response &request(const std::string &url,
                                  const T &body,
                                  const common::header_map &hdr=common::header_map())
        {
            if(prepare(url)) {
                the_request_.method=http_method::POST;
                // Default content type for HTML Forms
                the_request_.set_content_type("application/x-www-form-urlencoded");
                // Write URL encoded body into body stream
                url_encode(body, std::ostreambuf_iterator<char>(the_request_.body_stream()));
                the_client_->send_request(the_request_, the_response_);
            }
            return the_response_;
        }
        
    private:
        bool prepare(const std::string &url, const common::header_map &hdr=common::header_map());
        bool make_client(const std::string &host, uint16_t port);
        
        std::unique_ptr<client> the_client_;
        client::request the_request_;
        client::response the_response_;
    };
}}  // End of namespace fibio::http

#endif
