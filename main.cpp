// DNS Query Program on Linux
// Author : Constantino Ávarez Casado (arritmic@gmail.com)
// Dated : 26/6/2017

/*
 * Usage
 * =====
 *
 * dnsclient$ cat requests.in | ./dnsclient [-t|-u] 127.0.0.53
 * DNS Client [RO16_17]....
 * INFO: no transport chosen. Using UDP as default
 * Q: UDP 127.0.0.53 A balbla.com
 * A: 127.0.0.53 A 5 92.242.132.15
 * Q: UDP 127.0.0.53 NS google.com
 * A: 127.0.0.53 NS 5 ns1.google.com
 * A: 127.0.0.53 NS 5 ns4.google.com
 * A: 127.0.0.53 NS 5 ns3.google.com
 * A: 127.0.0.53 NS 5 ns2.google.com
 * Q: UDP 127.0.0.53 MX redhat.com
 * A: 127.0.0.53 NS 5 mx2.redhat.com
 * A: 127.0.0.53 NS 5 mx1.redhat.com
 * WARN: ignoring malformed line 3: 'this line is broken'
 * Q: UDP 127.0.0.53 A bbc.co.uk
 * A: 127.0.0.53 A 5 212.58.246.79
 * A: 127.0.0.53 A 5 212.58.244.22
 * A: 127.0.0.53 A 5 212.58.244.23
 * A: 127.0.0.53 A 5 212.58.246.78
 * WARN: ignoring malformed line 5: 'SUPER        broken.com'
 *
 * Implemented functionality
 * =========================
 *
 * - Only UDP transport.
 * - Only IPv4.
 *
 * Some notes
 * ==========
 *
 * Para validar resultados
 * -----------------------
 *
 * - MX records:
 *   nslookup -query=mx redhat.com
 *
 */

#include <arpa/inet.h>
#include <cassert>
#include <fstream>
#include <netinet/in.h>
#include <iostream>
#include <iterator>
#include <sstream>
#include <sys/socket.h>
#include <vector>
#include "AResourceRecord.h"
#include "NSResourceRecord.h"
#include "Message.h"

#define DEBUG_OUTPUT 1
#undef DEBUG_OUTPUT


using namespace ro1617;


std::vector<std::string> g_dns_servers; ///< IPs (in string form) from the DNS servers registered in the system


enum class TransportType
{
    TCP,
    UDP
};
TransportType g_transport_type;

/**
 * This is C++14. We're relying on RVO or move semantics for the return value.
 */
std::string to_string(TransportType tt)
{
    if (tt == TransportType::TCP)
    {
        return "TCP";
    }
    else if (tt == TransportType::UDP)
    {
        return "UDP";
    }
    else
    {
        throw std::logic_error("Unknown TransporType value");
    }
}

RRType to_rr_type(const std::string &str)
{
    if (str == "A")
        return RRType::A;
    else if (str == "MX")
        return RRType::MX;
    else if (str == "NS")
        return RRType::NS;
    else
        throw std::runtime_error("Unknown RR type '" + str + "'");
}

std::string to_string(RRType rr_type)
{
    switch (rr_type)
    {
        case RRType::A:
            return "A";
        case RRType::MX:
            return "MX";
        case RRType::NS:
            return "NS";
        default:
            throw std::logic_error("Don't know how to express given RRType");
    }
}


std::ostream &operator<<(std::ostream &os, const struct in_addr &addr)
{
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, str, INET_ADDRSTRLEN);
    os << str;
    return os;
}

std::ostream &operator<<(std::ostream &os, const AResourceRecord &a_rr)
{
    os << to_string(a_rr.rrtype()) << " " << a_rr.ttl() << " " << a_rr.getAddress();
    return os;
}

std::ostream &operator<<(std::ostream &os, const NSResourceRecord &ns_rr)
{
    os << to_string(ns_rr.rrtype()) << " " << ns_rr.ttl() << " " << toString(ns_rr.domain());
    return os;
}

std::ostream &operator<<(std::ostream &os, const ResourceRecord &rr)
{
    if (const auto *a_rr = dynamic_cast<const AResourceRecord *>(&rr))
    {
        os << *a_rr;
    }
#if 0
    else if (const auto *mx_rr = dynamic_cast<MXResourceRecord *>(&rr))
    {
        os << *mx_rr;
    }
#endif
    else if (const auto *ns_rr = dynamic_cast<const NSResourceRecord *>(&rr))
    {
        os << *ns_rr;

        /*
         * An MX RR will be treated like an NS one, as it derives from it.
         * The description of the practice only _speaks_ about a <valor> field. I won't be printing the PREFERENCE value, even if I have it.
         */
    }
    else
    {
        throw std::runtime_error("Unknown record type");
    }
    return os;
}


std::string rcode_to_string(unsigned int rcode)
{
    if (rcode == 0)
        return "No error";
    else if (rcode == 1)
        return "Format error";
    else if (rcode == 2)
        return "Server failure";
    else if (rcode == 3)
        return "Name Error";
    else if (rcode == 4)
        return "Not Implemented";
    else if (rcode == 5)
        return "Refused";
    return {};
                                                   }

/**
 * @brief Get the DNS servers from /etc/resolv.conf file on Linux
 */
void get_dns_servers()
{
    std::ifstream ifs{"/etc/resolv.conf"};
    if (!ifs.is_open())
    {
        std::cerr << "ERR: opening /etc/resolv.conf file\n";
    }

    std::regex nameserver_regex("^\\s*nameserver\\s+([\\d|.]+)");
    std::smatch match;

    std::string line;
    while (!ifs.eof())
    {
        std::getline(ifs, line);
        if (!std::regex_match(line, match, nameserver_regex))
        {
#ifdef DEBUG_OUTPUT
            std::cout << "DEBUG: ignoring resolve.conf line '" << line << "\n";
#endif
            continue;
        }
        auto sub_match = match[1];
        g_dns_servers.push_back(sub_match.str());
    }

#if 0
    g_dns_servers.push_back("208.67.222.222");
    g_dns_servers.push_back("208.67.220.220");
#endif
}


bool process_args(int argc, char *argv[])
{
    // Default values
    g_transport_type = TransportType::UDP;

    if (argc > 3 || argc < 2)
    {
        std::cerr << "ERR: wrong cmd line arguments\n";
        return false;
    }
    // Last arg is always the IP of the root NS server
    g_dns_servers.push_back(argv[argc-1]);
    struct in_addr ip_addr;
    if (inet_pton(AF_INET, g_dns_servers[0].c_str(), &ip_addr) == 0)
    {
        std::cerr << "ERR: invalid NS server IP address\n";
        return false;
    }
    // Consume it from the arguments
    argc -= 1;
    if (argc < 2)
    {
        std::cout << "INFO: no transport chosen. Using " << to_string(g_transport_type) << " as default\n";
        return true;
    }
    std::string transport_arg = argv[1];
    if (transport_arg == "-t" || transport_arg == "-u")
    {
        std::cout << "WARN: transport chosen but ignored (not impl.). Using " << to_string(g_transport_type) << " as default\n";
        return true;
    }
    else
    {
        std::cout << "ERR: unknown transport type '" << transport_arg << "'\n";
        return false;
    }
}

struct UserRequest
{
    RRType rr_type;
    std::string name;
};


/**
 * @return `true` if the line was successfully parsed, `false` otherwise
 */
bool parse_line(const std::string &line, UserRequest &user_request)
{
    std::istringstream iss{line};
    // istream_iterator skips ws by default
    std::vector<std::string> tokens{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};
    if (tokens.size() != 2)
    {
        return false;
    }
    // 1st token: record type
    try
    {
        user_request.rr_type = to_rr_type(tokens[0]);
    }
    catch (std::exception &e)
    {
        return false;
    }
    user_request.name = tokens[1];
    return true;
}


bool exec_query(const Message &query_msg, Message &ans)
{
    struct sockaddr_in addr;
    int s;

    if (query_msg.qdcount() == 0)
    {
        throw std::runtime_error("Input message contains no query. Cannot execute");
    }

    s = socket(AF_INET , SOCK_DGRAM , IPPROTO_UDP); //UDP packet for DNS queries
    if (s < 0)
    {
        throw std::system_error(errno, std::system_category());
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    addr.sin_addr.s_addr = inet_addr(g_dns_servers[0].c_str());

    std::vector<std::uint8_t> query_buf(query_msg.getRequiredBufferSize());
    std::uint8_t *buf_end = query_msg.toByteArray(query_buf.data(), query_buf.size());
    assert(buf_end > query_buf.data());
    assert(static_cast<size_t>(buf_end - &query_buf.front()) == query_buf.size());

#ifdef DEBUG_OUTPUT
    std::cout << "Sending request ...";
#endif
    if (sendto(s, query_buf.data(), query_buf.size(), 0, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        std::cerr << "ERR: sendto failed: " << strerror(errno) << "\n";
        return false;
    }
#ifdef DEBUG_OUTPUT
    std::cout << " Done\n";
#endif

    constexpr unsigned int RECV_BUF_SIZE = 65536;
    std::array<std::uint8_t, RECV_BUF_SIZE> recv_buf;
    socklen_t recv_addr_len = sizeof(addr);
    ssize_t recv_len;
#ifdef DEBUG_OUTPUT
    std::cout << "Receiving answer...";
#endif
    if ((recv_len = recvfrom(s, recv_buf.data(), RECV_BUF_SIZE, 0, reinterpret_cast<struct sockaddr *>(&addr) , &recv_addr_len)) < 0)
    {
        std::cerr << "ERR: recv failed: " << strerror(errno) << "\n";
        return false;
    }
#ifdef DEBUG_OUTPUT
    std::cout << " Done (" << recv_len << " bytes)\n";
#endif
    try
    {
        ans = makeMessage(recv_buf.data(), recv_buf.data() + recv_len);
    }
    catch (std::exception &e)
    {
        std::cerr << "ERR: broken ans: " << e.what() << "\n";
        return false;
    }
    return true;
}

int main(int argc, char *argv[])
{
    std::cout <<  "DNS Client [RO16_17].... " << std::endl;

    bool good_args = process_args(argc, argv);
    if (!good_args)
    {
        return EXIT_FAILURE;
    }

    // std::ifstream requests_stream{g_requests_file_path};
    // if (!requests_stream.is_open())
    // {
    //     std::cerr << "ERR: unable to open requests file '" << g_requests_file_path << "'\n";
    //     return EXIT_FAILURE;
    // }

    // get_dns_servers();
    // if (g_dns_servers.empty())
    // {
    //     std::cerr << "ERR: could not find any DNS server in the system\n";
    //     return EXIT_FAILURE;
    // }
    // std::cout << "INFO: using " << g_dns_servers[0] << " as server\n";

    unsigned int line_n = 0;
    std::istream &requests_stream = std::cin;
    while (!requests_stream.eof())
    {
        std::string line;
        std::getline(requests_stream, line);
        UserRequest user_request;
        bool line_ok = parse_line(line, user_request);
        if (!line_ok)
        {
            std::cerr << "WARN: ignoring malformed line " << line_n << ": '" << line << "'\n";
        }
        else
        {
            Message query_msg;
            query_msg.setQuestion(std::make_unique<DomainName>(user_request.name), user_request.rr_type);
            /*
             * Simplify our live and request recursion. Some servers might even refuse our request (per policy) without it.
             * This allows me to test easily against any DNS server.
             * http://www.simpledns.com/help/v50/index.html?df_recursion.htm
             */
            query_msg.setRd(true);
            std::cout << "Q: " << to_string(g_transport_type) << " " << g_dns_servers[0] << " "
                      << to_string(user_request.rr_type) << " " << user_request.name << "\n";
            Message ans;
            bool ok = exec_query(query_msg, ans);
            if (!ok)
            {
                std::cout << "INFO: could not get answer for query. Skipping (I'm not retrying)\n";
                continue;
            }
            if (ans.rcode())
            {
                std::cerr << "ERR: answer has error condition '" << rcode_to_string(ans.rcode()) << "'\n";
                continue;
            }
            // getAnswers() provides RRs in the Answer Section, while getNSRecords provides the ones in the Authority Section (authoritative servers)
            // (see https://stackoverflow.com/questions/26464348/dns-difference-between-ns-records-in-answer-section-and-ns-record-in-authority)
#ifdef DEBUG_OUTPUT
            std::cout << "    Questions: " << ans.qdcount() << "\n";
            std::cout << "    Answers: " << ans.ancount() << "\n";
#endif
            for (const auto &rr : ans.getAnswers())
            {
                std::cout << "A: " << g_dns_servers[0] << " " << *rr << "\n";
            }
#ifdef DEBUG_OUTPUT
            std::cout << "    Authority section count: " << ans.nscount() << "\n";
            std::cout << "    Additional section count: " << ans.adcount() << "\n";
#endif

        }
        line_n++;
    }

#if 0
    //Get the hostname from the terminal
    std::cout << "Enter RRType and Hostname to Lookup : ";
    std::getline(std::cin, input);

    std::string rrtype = input.substr(0, input.find(" "));
    std::string hostname = input.substr(input.find(" ")+1,input.size());
    std::cout << rrtype << " " << hostname << std::endl;

    //Now get the ip of this hostname , A record

    unsigned char *val = new unsigned char[hostname.length()+1];
    strcpy((char *)val,hostname.c_str());
    // do stuff

    int rrtypeInput = input_rr_type(rrtype);


    // It is not working with rrtypeInput = MX or NS or CNAME...
    ngethostbyname(val , rrtypeInput);
    delete [] val;
#endif

    return 0;
}
