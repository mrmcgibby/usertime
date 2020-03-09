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
private:
    bool m_logged_in = false;
    time_point m_login_time;
    duration m_login_duration;
    duration m_max_time = 0s;
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
    std::cout << str;
    for (auto u : json::parse(str))
    {
        std::string username = u["username"];
        int max_time = u["max_time"];
        user_times[username] =
            std::chrono::duration_cast<duration>(std::chrono::seconds(max_time));
        std::cout << "Limit user: " << username
                  << " to: " << max_time << " seconds" << std::endl;
    }
}

void check_for_violators()
{
    for (auto u : user_times)
    {
        if (u.second.logged_in() &&
            u.second.maxed_out())
        {
            std::cout << "Logging out user: " << u.first << std::endl;
            std::stringstream ss;
            ss << "pkill -u " << u.first;
            std::system(ss.str().c_str());
        }
        std::string warning =
            "zenity --warning --no-wrap --text=\"Your daily computer "
            "time is almost complete. You are about to be logged out.\"";
    }
}

int main(int argc, char** argv)
{
    read_config();
    while (true)
    {
        update();
        check_for_violators();
        std::this_thread::sleep_for(1s);
    }
}
