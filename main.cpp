#include <iostream>
#include <fstream>
#include <set>
#include <chrono>
#include <cassert>
#include <map>
#include <thread>
#include <algorithm>
#include <sstream>

#include "json.hpp"

using json = nlohmann::json;
using namespace std::literals;
using time_point = std::chrono::time_point<std::chrono::steady_clock>;
using duration = std::chrono::steady_clock::duration;

duration g_warning_time;
bool g_simulate = false;

class user_time
{
public:
    user_time() = default;
    user_time(duration max_time)
        : m_max_time(max_time)
    {
    }
    user_time(const user_time&) = default;
    user_time& operator=(const user_time&) = default;
    decltype(auto) total_time()
    {
        duration rval = m_login_duration;
        if (m_logged_in)
            rval += std::chrono::steady_clock::now() - m_login_time;
        return rval;
    }
    void login()
    {
        m_login_time = std::chrono::steady_clock::now();
        m_logged_in = true;
    }
    void logout()
    {
        if (!m_logged_in)
            return;
        m_login_duration += std::chrono::steady_clock::now() - m_login_time;
        m_logged_in = false;
    }
    bool logged_in()
    {
        return m_logged_in;
    }
    bool maxed_out()
    {
        return m_max_time != 0s
            && total_time() > m_max_time;
    }
    void warn()
    {
        if (m_max_time == 0s)
            return;
        if (m_max_time - total_time() > g_warning_time)
            return;
        if (m_warned)
            return;

        std::string warning =
            "zenity --warning --no-wrap --text=\"Your daily computer "
            "time is almost complete. You are about to be logged out.\" &";
        std::system(warning.c_str());
        m_warned = true;
    }
private:
    bool m_logged_in = false;
    time_point m_login_time;
    duration m_login_duration;
    duration m_max_time = 0s;
    bool m_warned = false;
};
std::map<std::string, user_time> user_times;

decltype(auto) new_users()
{
    std::set<std::string> users;

    std::system("users > usertime.txt");
    std::ifstream file("usertime.txt");
    while (file)
    {
        std::string u;
        file >> u;
        if (u.size() > 0)
            users.insert(u);
    }

    return users;
}

void update()
{
    auto users = new_users();
    // look for new users
    for (auto u : users)
    {
        auto it = std::find_if(user_times.begin(), user_times.end(), [&](auto&& ut){
            return ut.first == u && ut.second.logged_in();
        });
        if (it == user_times.end())
        {
            user_time& ut = user_times[u];
            ut.login();
            std::cout << u << " logged in" << std::endl;
        }
    }

    // look for users that have logged out
    for (auto it = user_times.begin(); it != user_times.end(); ++it)
    {
        if (users.find(it->first) == users.end()
            && it->second.logged_in())
        {
            it->second.logout();
            auto d = std::chrono::duration_cast<std::chrono::minutes>(it->second.total_time());
            std::cout << it->first << " logged out: "
                      << d.count() << " minutes"
                      << std::endl;
        }
    }
}

void read_config()
{
    std::ifstream file("config.json");
    std::string str{std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()};
    json conf = json::parse(str);
    for (auto u : conf["users"])
    {
        std::string username = u["username"];
        int max_time = u["max_time"];
        user_times[username] =
            std::chrono::duration_cast<duration>(std::chrono::seconds(max_time));
        std::cout << "Limit user: " << username
                  << " to: " << max_time << " seconds" << std::endl;
    }
    int wt = conf["warning_time"];
    g_warning_time = duration(wt);
    std::cout << "Warn user at: "
              << std::chrono::duration_cast<duration>(g_warning_time).count()
              << " seconds" << std::endl;
}

void force_logout(const std::string& username)
{
    std::cout << "Logging out user: " << username << std::endl;
    std::stringstream ss;
    ss << "pkill -u " << username;
    if (g_simulate)
        std::cout << ss.str() << std::endl;
    else
        std::system(ss.str().c_str());
}

void check_for_violators()
{
    for (auto u : user_times)
    {
        if (u.second.logged_in())
        {
            u.second.warn();
            if (u.second.maxed_out())
            {
                force_logout(u.first);
            }
        }
    }
}

int main(int argc, char** argv)
{
    if (argc == 2)
        g_simulate = (std::string(argv[1]) == "-s");
    read_config();
    while (true)
    {
        update();
        check_for_violators();
        std::this_thread::sleep_for(1s);
    }
}
