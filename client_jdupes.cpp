#include "beej-plus-plus/src/beej.h"

#include <iostream>

int main(int argc, char* argv[]) {
  beej::client client("localhost", 10001);
  if (argc != 2) return 1;
  client.connect();
  std::string line = std::string(argv[1]) + "\r\n";
  client.send(line);
  client.read([](const std::string& line) {
    std::cout << line << std::endl;
  });
}
