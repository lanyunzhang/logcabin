/* Copyright (c) 2012 Stanford University
 * Copyright (c) 2015 Diego Ongaro
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <getopt.h>
#include <iostream>
#include <string>

#include <LogCabin/Client.h>

namespace {

using LogCabin::Client::Cluster;
using LogCabin::Client::Configuration;
using LogCabin::Client::ConfigurationResult;
using LogCabin::Client::Result;
using LogCabin::Client::Server;
using LogCabin::Client::Status;

/**
 * Parses argv for the main function.
 */
class OptionParser {
  public:
    OptionParser(int& argc, char**& argv)
        : argc(argc)
        , argv(argv)
        , cluster("logcabin:5254")
        , fatal(false)
    {
        while (true) {
            static struct option longOptions[] = {
               {"cluster",  required_argument, NULL, 'c'},
               {"fatal",  no_argument, NULL, 'f'},
               {"seed",  required_argument, NULL, 's'},
               {"help",  no_argument, NULL, 'h'},
               {0, 0, 0, 0}
            };
            int c = getopt_long(argc, argv, "c:fhs:", longOptions, NULL);

            // Detect the end of the options.
            if (c == -1)
                break;

            switch (c) {
                case 'c':
                    cluster = optarg;
                    break;
                case 'h':
                    usage();
                    exit(0);
                case 'f':
                    fatal = true;
                    break;
                case 's':
                    srand(atoi(optarg));
                    break;
                case '?':
                default:
                    // getopt_long already printed an error message.
                    usage();
                    exit(1);
            }
        }

        // Additional command line arguments are not allowed.
        if (optind < argc) {
            usage();
            exit(1);
        }
    }

    void usage() {
        std::cout
            << "Test that repeatedly changes the cluster membership to random "
            << "subsets of its"
            << std::endl
            << "initial configuration."
            << std::endl
            << std::endl

            << "Usage: " << argv[0] << " [options]"
            << std::endl
            << std::endl

            << "Options:"
            << std::endl

            << "  -c <addresses>, --cluster=<addresses>  "
            << "Network addresses of the LogCabin"
            << std::endl
            << "                                         "
            << "servers, comma-separated"
            << std::endl
            << "                                         "
            << "[default: logcabin:5254]"
            << std::endl

            << "  -h, --help                     "
            << "Print this usage information"
            << std::endl

            << "  -f, --fatal                    "
            << "Exit on transient errors (continue by default)"
            << std::endl

            << "  -s <num>, --seed=<num>         "
            << "Random seed [default: 1]"
            << std::endl;
    }

    int& argc;
    char**& argv;
    std::string cluster;
    bool fatal;
};

void
printConfiguration(const std::pair<uint64_t, Configuration>& configuration)
{
    std::cout << "Configuration " << configuration.first << ":" << std::endl;
    for (auto it = configuration.second.begin();
         it != configuration.second.end();
         ++it) {
        std::cout << "- " << it->serverId << ": " << it->addresses
                  << std::endl;
    }
    std::cout << std::endl;
}

uint64_t
changeConfiguration(Cluster& cluster,
                    const Configuration& configuration,
                    uint64_t lastId,
                    bool fatal)
{
    std::cout << "Attempting to change cluster membership to the following:"
              << std::endl;
    for (auto it = configuration.begin(); it != configuration.end(); ++it) {
        std::cout << "- " << it->serverId << ": " << it->addresses
                  << std::endl;
    }

    ConfigurationResult result = cluster.setConfiguration(lastId,
                                                          configuration);
    std::cout << "Membership change result: ";
    if (result.status == ConfigurationResult::OK) {
        std::cout << "OK" << std::endl;
    } else if (result.status == ConfigurationResult::CHANGED) {
        std::cout << "CHANGED (" << result.error << ")" << std::endl;
    } else if (result.status == ConfigurationResult::BAD) {
        std::cout << "BAD SERVERS (" << result.error << "):" << std::endl;
        for (auto it = result.badServers.begin();
             it != result.badServers.end();
             ++it) {
            std::cout << "- " << it->serverId << ": " << it->addresses
                      << std::endl;
        }
    }
    std::cout << std::endl;

    std::cout << "Current configuration:" << std::endl;
     std::pair<uint64_t, Configuration> current = cluster.getConfiguration();
    printConfiguration(current);
    if (result.status != ConfigurationResult::OK && fatal) {
        std::cout << "Exiting" << std::endl;
        exit(1);
    }
    return current.first;
}

} // anonymous namespace

int
main(int argc, char** argv)
{
    OptionParser options(argc, argv);
    Cluster cluster(options.cluster);

    std::pair<uint64_t, Configuration> current =
        cluster.getConfiguration();
    std::cout << "Initial configuration:" << std::endl;
    printConfiguration(current);
    uint64_t lastId = current.first;
    Configuration& fullConfiguration = current.second;

    while (true) {
        Configuration newConfiguration;
        uint64_t desiredServers =
            (rand() % fullConfiguration.size()) + 1; // NOLINT
        Configuration remainingServers = fullConfiguration;
        for (uint64_t i = 0; i < desiredServers; ++i) {
            uint64_t j = (rand() % remainingServers.size()); // NOLINT
            newConfiguration.push_back(remainingServers.at(j));
            std::swap(remainingServers.at(j),
                      remainingServers.back());
            remainingServers.pop_back();
        }
        lastId = changeConfiguration(cluster, newConfiguration, lastId,
                                     options.fatal);
    }
}
