#include "cpp_reducer.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <exception>

using namespace std;

using recipient_map = map<string, uint32_t>;
using sender_map = map<string, recipient_map>;

ostream &operator<<(ostream &os, const recipient_map &recipients);
ostream &operator<<(ostream &os, const sender_map &senders);

//extern "C" {
//  void cpp_reducer(char *input, char *output) {
//    cpp_as_cpp_files_reducer(input, output);
//  }
//}

void /*cpp_as_*/cpp_files_reducer(char *input, char *output) {
  sender_map result{};

  if (!input || !output)
    return;

  ifstream step2_file{input};
  ofstream result_file{output};
  if (!step2_file.is_open() || !result_file.is_open()) {
    cout << "Could not open either " << input << " or " << output << endl;
    return;
  }

  uint32_t line_read = 0;
  for (string line; getline(step2_file, line); ) {
    ++line_read;
    stringstream s{line};
    string sender;
    s >> sender;
    pair<sender_map::iterator, bool> r = result.insert({sender, {}});
    while (!s.eof()) {
      string current_recipient;
      s >> current_recipient;
      recipient_map &recipients = r.first->second;
      try {
        auto &item = recipients.at(current_recipient);
        item += 1;
      } catch (out_of_range &e) {
        recipients.insert({current_recipient, 1});
      }
    }
  }
  result_file << result;
}

ostream &operator<<(ostream &os, const recipient_map &recipients) {
  for (const auto &item: recipients) {
    os << " " << item.second << ":" << item.first;
  }
  return os;
}

ostream &operator<<(ostream &os, const sender_map &senders) {
  for (const auto &item: senders) {
    os << item.first << ":" << item.second << endl;
  }
  return os;
}

