#include <mutex>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <cstring>
#include <deque>
#include <set>
#include <thread>
#include <functional>
#include <atomic>
#include <future>
#include <random>

#pragma once

#ifdef  UNDER_GTEST
#define GTEST_ONLY(expr) expr
#else
#define GTEST_ONLY(expr)
#endif

#define UNUSED(expr) do { (void)(expr); } while (0);
#define MILLISECONDS(ms) (std::chrono::milliseconds(ms))
#define NANOSECONDS(ms)  (std::chrono::nanoseconds(ms))
#define KB(n) (n * (0x1<<10))
#define MB(n) (n * (0x1<<20))
#define GB(n) (n * (0x1<<30))

#define OUT(...) do { \
    safe_print(string_format(__VA_ARGS__) + " at %s:%d ", __FILE__, __LINE__); \
} while(0);

#define ERR(...) do { \
    errout(std::string("ERR: ") + string_format(__VA_ARGS__) + " errno=%d(%s) at %s:%d", errno, std::strerror(errno), __FILE__, __LINE__); \
} while(0);

#define SLEEP_MS(ms) do { \
    std::this_thread::sleep_for(MILLISECONDS(ms)); \
} while(0);

#if defined(  __x86_64__ )
#define ALWAYS_BREAK()          __asm__("int $3")
#else
#define ALWAYS_BREAK()          ::abort()
#endif

inline static void __assertFunction( const char *message, const char * file, int line )
{
    std::cerr << "ASSERT:" << file << "(" << line << ") " << message << " " << errno << "(" << std::strerror(errno) << ") ";
    ALWAYS_BREAK();
}

#define ASSUME(must_be_true_predicate,msg)      \
    ((must_be_true_predicate)                   \
    ? (void )0                                  \
    : __assertFunction(msg,__FILE__,__LINE__))


std::mutex g_pub_mu_cout;

class nv_exception : public std::exception {
    int   _errno;
    std::string _errstr;
    std::string _whatmsg;
public:
    nv_exception(std::string msg) noexcept : _errno(errno), _errstr(std::strerror(errno))
    {
        _whatmsg = msg + "(errno(" + std::to_string(errno) + "): " + _errstr + ")";
    }
    const char* what() const noexcept {
        return _whatmsg.c_str();
    }
};

class timeout_exception : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

bool g_printtime = false;
void setprinttime(bool prt = false) {
    g_printtime = prt;
}

template<typename ... Args>
std::string string_format( const std::string& format, Args ... args )
{
    int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
    auto size = static_cast<size_t>( size_s );
    std::unique_ptr<char[]> buf( new char[size] );
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

template<typename ... Args>
void safe_print( std::string format, Args ... args )
{
    std::string t("");
    thread_local static std::chrono::steady_clock::time_point last_print_time = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> locker(g_pub_mu_cout);
    if(g_printtime) {
        t = string_format("(+%.4dms) ", (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - last_print_time)).count());
    }
    std::string s = t + string_format(format, args ... );
    std::cout << s << std::endl;
    last_print_time = std::chrono::steady_clock::now();
}

template<typename ... Args>
void errout( std::string format, Args ... args )
{
    std::string t("");
    thread_local static std::chrono::steady_clock::time_point last_print_time = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> locker(g_pub_mu_cout);
    if(g_printtime) {
        t = string_format("(+%.4dms) ", (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - last_print_time)).count());
    }
    std::string s = t + string_format(format, args ... );
    std::cerr << s << std::endl;
    last_print_time = std::chrono::steady_clock::now();
}

/**
 * @brief random funciton.
 * 
 * @param pct the probability of returning true (in percentage)
 * @return true if hit, else false
 */
bool dice(int pct=0) {
    static unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    static std::default_random_engine generator(seed);
    std::uniform_int_distribution<int> distribution(0,99);
    int number = distribution(generator);
    return (number < pct);
}

std::string uuid() {
    static std::random_device dev;
    static std::mt19937 rng(dev());

    std::uniform_int_distribution<int> dist(0, 15);

    const char *v = "0123456789abcdef";
    const bool dash[] = { 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0 };

    std::string res;
    for (int i = 0; i < 16; i++) {
        if (dash[i]) res += "-";
        res += v[dist(rng)];
        res += v[dist(rng)];
    }
    return res;
}

