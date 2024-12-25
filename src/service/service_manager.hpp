#ifndef RXREVOLTCHAIN_SERVICE_SERVICE_MANAGER_HPP
#define RXREVOLTCHAIN_SERVICE_SERVICE_MANAGER_HPP

#include <string>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <mutex>
#include "../util/logger.hpp"
#include "request.hpp"
#include "response.hpp"

/**
 * @file service_manager.hpp
 * @brief Coordinates logic among request/response, acting like an RPC or REST router.
 *
 * DESIGN GOALS:
 *   - Provide a place to register "handlers" for specific methods/routes (e.g. "GET /ping"),
 *     each returning a Response when given a Request.
 *   - handleRequest(Request) -> Response: looks up the corresponding handler and invokes it.
 *   - If no handler is found, return a 404-like response.
 *   - This is a naive in-memory router; no actual HTTP listening is done. 
 *     An external "server" might parse HTTP, build a Request, then call handleRequest().
 *
 * USAGE EXAMPLE:
 *   @code
 *   using namespace rxrevoltchain::service;
 *
 *   ServiceManager manager;
 *   // Register a handler
 *   manager.registerHandler("GET", "/ping", [](const Request &req){
 *       return Response(200, "OK", "PONG");
 *   });
 *
 *   // Suppose we parse an incoming Request:
 *   Request req;
 *   req.method = "GET";
 *   req.route  = "/ping";
 *   // ...
 *   Response resp = manager.handleRequest(req);
 *   std::string jsonOut = resp.toJson();
 *   @endcode
 */

namespace rxrevoltchain {
namespace service {

/**
 * @typedef HandlerFn
 * @brief A function type that takes a Request and returns a Response.
 */
using HandlerFn = std::function<Response(const Request &)>;

/**
 * @class ServiceManager
 * @brief Maintains a mapping of (method + route) -> handler function, 
 *        dispatches requests to the correct handler, and returns responses.
 */
class ServiceManager
{
public:
    ServiceManager() = default;
    ~ServiceManager() = default;

    /**
     * @brief Register a handler function for a specific method and route.
     * @param method e.g. "GET", "POST".
     * @param route e.g. "/ping", "/submitEOB".
     * @param handler A function that takes a Request and returns a Response.
     * @throw std::runtime_error if this (method,route) is already registered.
     */
    inline void registerHandler(const std::string &method,
                                const std::string &route,
                                const HandlerFn &handler)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = makeKey(method, route);
        if (handlers_.find(key) != handlers_.end()) {
            throw std::runtime_error("ServiceManager: (method, route) already registered: " 
                                     + method + " " + route);
        }
        handlers_[key] = handler;
        rxrevoltchain::util::logger::debug("ServiceManager: Registered handler for " + method + " " + route);
    }

    /**
     * @brief Handle an incoming request by dispatching to the correct handler,
     *        or return a 404-like response if none is found.
     * @param req The incoming request object (method, route, data, etc.).
     * @return A Response generated by the matched handler, or a 404 if no match.
     */
    inline Response handleRequest(const Request &req)
    {
        std::string key = makeKey(req.method, req.route);
        HandlerFn handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = handlers_.find(key);
            if (it == handlers_.end()) {
                // No such handler => return 404 response
                rxrevoltchain::util::logger::warn("ServiceManager: No handler for " 
                                                   + req.method + " " + req.route 
                                                   + ", returning 404.");
                return Response(404, "Not Found", 
                                "No handler for " + req.method + " " + req.route);
            }
            handler = it->second;
        }

        // Call the handler
        return handler(req);
    }

private:
    // Composite key = method + "|" + route
    inline std::string makeKey(const std::string &method, const std::string &route) const
    {
        return method + "|" + route;
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, HandlerFn> handlers_;
};

} // namespace service
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_SERVICE_SERVICE_MANAGER_HPP
