#ifndef WEBSOCKETPP_ASIO_COMPATIBILITY_H
#define WEBSOCKETPP_ASIO_COMPATIBILITY_H

#include <boost/version.hpp>
#include <boost/asio.hpp>

// Define compatibility layer for WebSocketPP with newer Boost/ASIO versions
namespace websocketpp {
    namespace lib {
        namespace asio {
            // In newer Boost versions, io_service was renamed to io_context
            #if BOOST_VERSION >= 106600 // Boost 1.66.0 and newer
                typedef boost::asio::io_context io_service;
                
                // Create a compatibility class for steady_timer
                template <typename Clock>
                class basic_waitable_timer_wrapper : public boost::asio::basic_waitable_timer<Clock> {
                public:
                    // Constructor that accepts io_service and duration
                    basic_waitable_timer_wrapper(io_service& io, const typename Clock::duration& duration)
                        : boost::asio::basic_waitable_timer<Clock>(io, Clock::now() + duration) {}
                    
                    // Constructor that accepts io_service and time_point
                    basic_waitable_timer_wrapper(io_service& io, const typename Clock::time_point& time_point)
                        : boost::asio::basic_waitable_timer<Clock>(io, time_point) {}
                };
                
                // Define steady_timer with the wrapper
                typedef basic_waitable_timer_wrapper<boost::asio::chrono::steady_clock> steady_timer;
                
                // Define milliseconds to use std::chrono
                using milliseconds = std::chrono::milliseconds;
                
                // Add resolver query compatibility
                // In Boost 1.66+, query type was removed, so we create a compatibility class
                namespace ip {
                    class query {
                    public:
                        enum flags {
                            address_configured = boost::asio::ip::resolver_base::address_configured,
                            canonical_name = boost::asio::ip::resolver_base::canonical_name,
                            passive = boost::asio::ip::resolver_base::passive,
                            numeric_host = boost::asio::ip::resolver_base::numeric_host,
                            numeric_service = boost::asio::ip::resolver_base::numeric_service,
                            v4_mapped = boost::asio::ip::resolver_base::v4_mapped,
                            all_matching = boost::asio::ip::resolver_base::all_matching,
                            numeric_service_v4 = all_matching
                        };
                        
                        query(const std::string& host, const std::string& service)
                            : host_(host), service_(service), flags_(0) {}
                            
                        query(const std::string& host, const std::string& service, int flags)
                            : host_(host), service_(service), flags_(flags) {}
                            
                        const std::string& host() const { return host_; }
                        const std::string& service() const { return service_; }
                        int flags() const { return flags_; }
                        
                    private:
                        std::string host_;
                        std::string service_;
                        int flags_;
                    };
                
                    // TCP resolver compatibility
                    namespace tcp {
                        // Add resolver compatibility
                        typedef boost::asio::ip::tcp::resolver resolver;
                        
                        // Add acceptor compatibility
                        typedef boost::asio::ip::tcp::acceptor acceptor;
                        
                        // Add resolver iterator compatibility
                        typedef boost::asio::ip::tcp::resolver::results_type resolver_iterator;
                        
                        // Define end iterator for compatibility
                        inline resolver_iterator resolver_iterator_end() {
                            return resolver_iterator();
                        }
                        
                        // Add resolver extension for compatibility with older code
                        inline resolver_iterator resolve(resolver& resolver_instance, const ip::query& query) {
                            return resolver_instance.resolve(query.host(), query.service());
                        }
                    };
                }
                
                // Define a global end function for resolver iterators
                inline ip::tcp::resolver_iterator resolver_iterator_end() {
                    return ip::tcp::resolver_iterator();
                }
                
                // Define strand for compatibility
                using io_service_ns = boost::asio;
                
                // Define strand type for io_service
                class io_service_strand {
                private:
                    boost::asio::strand<boost::asio::io_context::executor_type> strand_impl;
                    
                public:
                    explicit io_service_strand(io_service& ios)
                        : strand_impl(boost::asio::make_strand(ios)) {}
                    
                    template <typename Handler>
                    auto wrap(Handler&& handler) {
                        return boost::asio::bind_executor(strand_impl, std::forward<Handler>(handler));
                    }
                };
                
                // Define strand for io_service
                typedef io_service_strand strand;
                
                // Define work class for io_service
                class io_service_work {
                private:
                    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_impl;
                    
                public:
                    explicit io_service_work(io_service& ios)
                        : work_impl(boost::asio::make_work_guard(ios)) {}
                };
                
                // Add work type for io_service
                typedef io_service_work io_service_work_type;
                
                // Define work alias in io_service_ns namespace
                namespace boost_asio_work_ns {
                    typedef io_service_work work;
                }
            #endif
        }
    }
}

// Define a global end variable for compatibility with older code
#if BOOST_VERSION >= 106600
    namespace {
        const auto& end = websocketpp::lib::asio::resolver_iterator_end();
    }
#endif

#endif // WEBSOCKETPP_ASIO_COMPATIBILITY_H