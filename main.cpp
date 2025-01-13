#include <iostream>
#include <string>
#include <cstdlib>
#include "sender.hpp"
#include "receiver.hpp"

using namespace std;

int main(int argc, char *argv[]) {
    string host = "localhost";
    int port = 0;

    if (argc == 3) {
        host = argv[1];
        port = stoi(argv[2]);
    } else if (argc == 2) {
        port = stoi(argv[1]);
    } else {
        cerr << "Usage: " << argv[0] << " [host] [port]" << endl;
        return EXIT_FAILURE;
    }

    if (host == "localhost") {
        host = "127.0.0.1";
    }

    cout << "[i] Node started at " << host << ":" << port << endl;
    cout << "[?] Please choose the operating mode" << endl;
    cout << "[?] 1. Sender" << endl;
    cout << "[?] 2. Receiver" << endl;
    cout << "[?] Input: ";

    int mode;
    cin >> mode;
    cin.ignore();

    if (mode == 1) {
        Sender sender(host, port);
        sender.run();
    } else if (mode == 2) {
        string senderIp;
        cout << "[?] Input the sender's IP address: ";
        getline(cin, senderIp);
        Receiver receiver(host, port, senderIp);
        receiver.run();
    } else {
        cerr << "[-] Invalid mode selected." << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}