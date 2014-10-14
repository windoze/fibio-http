//
//  url_parser.h
//  fibio-http
//
//  Created by Chen Xu on 14/10/13.
//  Copyright (c) 2014 0d0a.com. All rights reserved.
//

#ifndef fibio_http_url_parser_hpp
#define fibio_http_url_parser_hpp

#include <map>
#include <vector>
#include <string>

namespace fibio { namespace http { namespace common {
    inline bool hex_digit_to_int(char c, int &ret) {
        if (c>='0' && c<='9') {
            ret=c-'0';
        } else if (c>='a' && c<='f') {
            ret=c-'a';
        } else if (c>='A' && c<='Z') {
            ret=c-'A';
        } else {
            return false;
        }
        return true;
    }
    
    template<typename Iterator>
    bool hex_to_char(Iterator i, char &ret) {
        int hi, lo;
        if (hex_digit_to_int(*i, hi) && hex_digit_to_int(*(i+1), lo)) {
            ret=static_cast<char>(hi << 4 | lo);
            return true;
        }
        return false;
    }
    
    template<typename Iterator, typename OutputIterator>
    bool url_decode(Iterator in_begin,
                    Iterator in_end,
                    OutputIterator out)
    {
        size_t in_size=in_end-in_begin;
        for (std::size_t i = 0; i < in_size; ++i) {
            if (*(in_begin+i) == '%') {
                if (i + 3 <= in_size) {
                    char c;
                    if (hex_to_char(in_begin+i+1, c)) {
                        *out++ = c;
                        i+=2;
                    } else {
                        return false;
                    }
                } else {
                    return false;
                }
            } else if (*(in_begin+i) == '+') {
                *out++ = ' ';
            } else {
                *out++ = *(in_begin+i);
            }
        }
        return true;
    }
    
    bool parse_path_components(const std::string &path, std::vector<std::string> &components);
    bool parse_query_string(const std::string &query, std::map<std::string, std::string> &parameters);
}}} // End of namespace fibio::http::common

#endif /* defined(fibio_http_url_parser_hpp) */
