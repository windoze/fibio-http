//
//  server.cpp
//  fibio
//
//  Created by Chen Xu on 14-3-13.
//  Copyright (c) 2014 0d0a.com. All rights reserved.
//

#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/iostreams/restrict.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <fibio/future.hpp>
#include <fibio/http/server/server.hpp>

namespace fibio { namespace http {
    namespace detail {
        typedef fibio::http::server_request request;
        typedef fibio::http::server_response response;
        typedef boost::asio::basic_waitable_timer<std::chrono::steady_clock> watchdog_timer_t;
        
        struct connection {
            connection()=default;
            connection(connection &&other)=default;
            connection(const connection &)=delete;
            ~connection() {
                close();
            }
            
            void start_watchdog() {
                watchdog_timer_.reset(new watchdog_timer_t(asio::get_io_service()));
                watchdog_fiber_.reset(new fiber(fiber::attributes(fiber::attributes::stick_with_parent),
                                                &connection::watchdog_fiber,
                                                this));
            }
            
            void watchdog_fiber() {
                boost::system::error_code ignore_ec;
                while (is_open()) {
                    watchdog_timer_->async_wait(asio::yield[ignore_ec]);
                    // close the stream if timeout
                    auto dur=watchdog_timer_->expires_from_now();
                std::chrono::seconds s=std::chrono::duration_cast<std::chrono::seconds>(dur);
                    if (s <= std::chrono::seconds(0)) {
                        stream_.close();
                    }
                }
            }
            
            bool recv(request &req) {
                bool ret=false;
                if (!stream_.is_open() || stream_.eof() || stream_.fail() || stream_.bad()) return false;
                if(read_timeout_>std::chrono::seconds(0)) {
                    // Set read timeout
                    watchdog_timer_->expires_from_now(read_timeout_);
                }
                ret=req.read(stream_);
                return ret;
            }
            
            bool send(response &resp) {
                bool ret=false;
                if (!stream_.is_open() || stream_.eof() || stream_.fail() || stream_.bad()) return false;
                if(write_timeout_>std::chrono::seconds(0)) {
                    // Set write timeout
                    watchdog_timer_->expires_from_now(write_timeout_);
                }
                ret=resp.write(stream_);
                if (!resp.keep_alive) {
                    stream_.close();
                    return false;
                }
                return ret;
            }
            
            bool is_open() const { return stream_.is_open(); }
            
            void close() {
                stream_.close();
                if (watchdog_timer_) {
                    watchdog_timer_->cancel();
                }
                if (watchdog_fiber_) {
                    watchdog_fiber_->join();
                    watchdog_fiber_.reset();
                }
                watchdog_timer_.reset();
            }
            
            std::string host_;
            stream::tcp_stream stream_;
            timeout_type read_timeout_=std::chrono::seconds(0);
            timeout_type write_timeout_=std::chrono::seconds(0);
            std::unique_ptr<watchdog_timer_t> watchdog_timer_;
            std::unique_ptr<fiber> watchdog_fiber_;
        };
        
        struct server_engine {
            server_engine(const std::string &addr,
                          unsigned short port,
                          const std::string &host,
                          server::request_handler_type default_request_handler)
            : host_(host)
            , acceptor_(addr.c_str(), port)
            , default_request_handler_(std::move(default_request_handler))
            , active_connection_(0)
            {}

            server_engine(unsigned short port, const std::string &host)
            : host_(host)
            , acceptor_(port)
            {}
            
            void start() {
                watchdog_.reset(new fiber(fiber::attributes(fiber::attributes::stick_with_parent),
                                          &server_engine::watchdog,
                                          this));
                boost::system::error_code ec;
                // Loop until accept closed
                while (true) {
                    connection sc;
                    ec=accept(sc);
                    if(ec) break;
                    sc.read_timeout_=read_timeout_;
                    sc.write_timeout_=write_timeout_;
                    fiber(&server_engine::servant, this, std::move(sc)).detach();
                }
                watchdog_->join();
            }
            
            void close() {
                exit_signal_.set_value();
                if(watchdog_)
                    watchdog_->join();
                // Wait until all connections are closed
                std::unique_lock<mutex> l(connection_counter_mtx_);
                while(active_connection_) {
                    connection_close_.wait(l);
                }
            }
            
            boost::system::error_code accept(connection &sc) {
                boost::system::error_code ec;
                acceptor_(sc.stream_, ec);
                if (!ec) {
                    sc.host_=host_;
                    active_connection_++;
                }
                return ec;
            }
            
            void watchdog() {
                exit_signal_.get_future().wait();
                acceptor_.close();
            }
            
            void servant(connection c) {
                if (read_timeout_>std::chrono::seconds(0) || write_timeout_>std::chrono::seconds(0)) {
                    c.start_watchdog();
                }
                request req;
                int count=0;
                while(c.recv(req)) {
                    response resp;
                    // Set default attributes for response
                    resp.status_code=http_status_code::OK;
                    resp.version=req.version;
                    resp.keep_alive=req.keep_alive;
                    if(count>=max_keep_alive_) resp.keep_alive=false;
                    if(!dispatch_vhost(req, resp, c)) {
                        break;
                    }
                    c.send(resp);
                    // Make sure we consumed all parts of the request
                    req.drop_body();
                    // Make sure all data are received and sent
                    c.stream_.flush();
                    // Keepalive counter
                    count++;
                }
                c.close();
                
                active_connection_--;
                connection_close_.notify_one();
            }
            
            bool dispatch_vhost(request &req, response &resp, connection &c) {
                if(req.method==http_method::INVALID)
                    return false;
                // Get "host" header
                auto i=req.headers.find("host");
                if(i!=req.headers.end()) {
                    // Find virtual host
                    // TODO: Host may be in host:port format, need to split
                    auto j=vhosts_.find(i->second);
                    if(j!=vhosts_.end()) {
                        return j->second(req, resp, c.stream_);
                    }
                }
                return default_request_handler_(req, resp, c.stream_);
            }
            
            std::string host_;
            tcp_stream_acceptor acceptor_;
            server::request_handler_type default_request_handler_;
            promise<void> exit_signal_;
            timeout_type read_timeout_=std::chrono::seconds(0);
            timeout_type write_timeout_=std::chrono::seconds(0);
            unsigned max_keep_alive_=DEFAULT_KEEP_ALIVE_REQ_PER_CONNECTION;
            
            // TODO: Use a more efficient data structure
            typedef std::map<std::string, server::request_handler_type, common::iless> vhost_map;
            // TODO: Do we need a mutex here?
            vhost_map vhosts_;
            
            std::unique_ptr<fiber> watchdog_;
            
            // connection uses vhost info in server
            // make sure server exists if there is living connection
            std::atomic<uint32_t> active_connection_;
            mutex connection_counter_mtx_;
            condition_variable connection_close_;
        };
    }   // End of namespace detail
    
    //////////////////////////////////////////////////////////////////////////////////////////
    // server_request
    //////////////////////////////////////////////////////////////////////////////////////////
    
    void server_request::clear() {
        // Make sure there is no pending data in the last request
        drop_body();
        common::request::clear();
    }
    
    bool server_request::accept_compressed() const {
        auto i=headers.find("Accept-Encoding");
        if (i==headers.end()) return false;
        // TODO: Kinda buggy
        return strcasestr(i->second.c_str(), "gzip")!=NULL;
    }
    
    bool server_request::read(std::istream &is) {
        clear();
        if (!common::request::read_header(is)) return false;
        if (content_length>0) {
            // Setup body stream
            namespace bio = boost::iostreams;
            restriction_.reset(new bio::restriction<std::istream>(is, 0, content_length));
            bio::filtering_istream *in=new bio::filtering_istream;
            in->push(*restriction_);
            body_stream_.reset(in);
        }
        return true;
    }
    
    void server_request::drop_body() {
        // Discard body content iff body stream exists
        if (body_stream_) {
            while (!body_stream().eof()) {
                char buf[1024];
                body_stream().read(buf, sizeof(buf));
            }
            body_stream_.reset();
            restriction_.reset();
        }
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////
    // server_response
    //////////////////////////////////////////////////////////////////////////////////////////
    
    void server_response::clear() {
        common::response::clear();
        std::string e;
        if (!raw_body_stream_.vector().empty())
            raw_body_stream_.swap_vector(e);
    }
    
    const std::string &server_response::get_body() const {
        return raw_body_stream_.vector();
    }
    
    std::ostream &server_response::body_stream() {
        return raw_body_stream_;
    }
    
    size_t server_response::get_content_length() const {
        return raw_body_stream_.vector().size();
    }
    
    void server_response::set_content_type(const std::string &ct) {
        auto i=headers.find("content-type");
        if (i==headers.end()) {
            headers.insert(std::make_pair("Content-Type", ct));
        } else {
            i->second.assign(ct);
        }
    }
    
    bool server_response::write_header(std::ostream &os) {
        std::string ka;
        if (keep_alive) {
            ka="keep-alive";
        } else {
            ka="close";
        }
        auto i=headers.find("connection");
        if (i==headers.end()) {
            headers.insert(std::make_pair("Connection", ka));
        } else {
            i->second.assign(ka);
        }
        if (!common::response::write_header(os)) return false;
        return !os.eof() && !os.fail() && !os.bad();
    }
    
    bool server_response::write(std::ostream &os) {
        // Set "content-length" header
        auto i=headers.find("content-length");
        if (i==headers.end()) {
            headers.insert(std::make_pair("Content-Length", boost::lexical_cast<std::string>(get_content_length())));
        } else {
            i->second.assign(boost::lexical_cast<std::string>(get_content_length()));
        }
        // Write headers
        if (!write_header(os)) return false;
        // Write body
        os.write(&(raw_body_stream_.vector()[0]), raw_body_stream_.vector().size());
        return !os.eof() && !os.fail() && !os.bad();
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // server
    //////////////////////////////////////////////////////////////////////////////////////////

    static std::string get_default_host_name() {
        // TODO: Get default host name
        return "127.0.0.1";
    }
    
    void server::impl_deleter::operator()(detail::server_engine *p) {
        delete p;
    }
    
    server::server(settings s)
    : engine_(new detail::server_engine(s.address,
                                        s.port,
                                        get_default_host_name(),
                                        std::move(s.default_request_handler)))
    {
        engine_->read_timeout_=s.read_timeout;
        engine_->write_timeout_=s.write_timeout;
        engine_->max_keep_alive_=s.max_keep_alive;
    }
    
    server::~server() {
        stop();
    }
    
    void server::start() {
        servant_.reset(new fiber(&detail::server_engine::start, engine_.get()));
    }
    
    void server::stop() {
        if (servant_) {
            engine_->close();
        }
    }
    
    void server::join() {
        if (servant_) {
            servant_->join();
            servant_.reset();
        }
    }
    
    void server::add_virtual_host(const std::string &vhost, request_handler_type &&handler) {
        engine_->vhosts_[boost::to_lower_copy(vhost)]=std::move(handler);
    }
    
    void server::remove_virtual_host(const std::string &vhost) {
        engine_->vhosts_.erase(boost::to_lower_copy(vhost));
    }
    
    void server::set_default_request_handler(request_handler_type &&handler) {
        engine_->default_request_handler_=std::move(handler);
    }
    
    void server::set_request_handler(const std::string &vhost, request_handler_type &&handler) {
        auto i=engine_->vhosts_.find(vhost);
        if (i!=engine_->vhosts_.end()) {
            i->second=std::move(handler);
        }
    }
}}  // End of namespace fibio::http
