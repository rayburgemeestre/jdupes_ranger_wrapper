#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "beej-plus-plus/src/beej.h"
#include "json.hpp"

namespace fs = std::filesystem;
using namespace std::literals::string_literals;
using json = nlohmann::json;

class parser {
private:
  std::unordered_map<uint64_t, std::string> id_to_file;
  std::unordered_map<std::string, uint64_t> file_to_id;
  std::unordered_map<uint64_t, uint64_t> id_to_group_id;
  std::unordered_map<uint64_t, std::vector<uint64_t>> group_id_groups;
  std::unordered_map<uint64_t, uint64_t> group_id_bytes;

  std::set<std::string> deleted_files;
  std::set<std::string> deleted_directories;

  uint64_t unique_file_id = 0;
  uint64_t unique_group_id = 0;

  enum class states {
    reading_bytes,
    reading_files,
  } state;

public:
  parser();
  uint64_t file(const std::string& filename);
  std::string query(const std::string& filename);
};

parser::parser() : state(states::reading_bytes) {
  // scan deleted files and directories
  std::ifstream fi2;
  char* homedir = getenv("HOME");
  fi2.open(std::string(homedir) + "/ranger_queued_delete.txt");
  std::string deleted_file;

  while (getline(fi2, deleted_file, '\n')) {
    std::cout << "deleted = " << deleted_file << std::endl;
    const fs::path sandbox{deleted_file};
    if (fs::is_regular_file(fs::status(sandbox))) {
      deleted_files.insert(deleted_file);
    } else if (fs::is_directory(fs::status(sandbox))) {
      deleted_directories.insert(deleted_file);
    }
  }

  //--

  std::ifstream fi;
  fi.open("jdupes.txt");
  std::string line;
  size_t bytes = 0;
  std::vector<uint64_t> group;

  auto process_line = [&]() {
    group_id_groups[unique_group_id] = group;  // copy
    group_id_bytes[unique_group_id] = bytes;
    state = states::reading_bytes;
    group.clear();
    unique_group_id++;
  };

  while (getline(fi, line, '\n')) {
    switch (state) {
      case states::reading_bytes:
        bytes = std::atoi(line.c_str());
        state = states::reading_files;
        break;
      case states::reading_files:
        if (line.empty()) {
          process_line();
        } else {
          // if (deleted_files.find(line) == deleted_files.end() &&
          //    deleted_directories.find(fs::path(line).parent_path().string()) == deleted_directories.end()
          //){
          // add this one to the group
          uint64_t id = file(line);
          group.push_back(id);
          id_to_group_id[id] = unique_group_id;
          //}
        }
        break;
    }
  }
  if (state == states::reading_files && !group.empty()) {
    process_line();
  }
}

uint64_t parser::file(const std::string& filename) {
  auto find = file_to_id.find(filename);
  if (find != file_to_id.end()) {
    // Cannot happen??
    return find->second;
  }
  file_to_id[filename] = unique_file_id;
  id_to_file[unique_file_id] = filename;
  return unique_file_id++;
}

std::string parser::query(const std::string& filename) {
  auto find = file_to_id.find(filename);
  if (find == file_to_id.end()) {
    return "";
  }

  auto id = file_to_id[filename];
  auto gid = id_to_group_id[id];
  auto bytes = group_id_bytes[gid];

  json file;
  file["file"] = filename;
  file["bytes"] = bytes;
  std::vector<std::string> dupes;
  const auto parent_dir_deleted = [&](const std::string& file){
    auto current = fs::path(file).parent_path();
    while (current.string() != "/") {
      std::cout << "checking dir: " << current.string() << std::endl;
      if (deleted_directories.find(current.string()) != deleted_directories.end()) {
        std::cout << "directory was deleted: " << current.string() << std::endl;
        return true; // this dir was deleted.
      }
      else if(!fs::exists(current)) {
        std::cout << "Dir does not exist: " << current.string() << std::endl;
        return true; // this dir does not exist.
      }

      current = current.parent_path();
    }
    std::cout << "Dir was not deleted: " << current.string() << std::endl;
    return false;
  };
  const auto file_deleted = [&](const std::string& file) {
    if (deleted_files.find(file) != deleted_files.end()) {
      std::cout << "File was deleted: " << file << std::endl;
      return true; // file was deleted
    }
    else if(!fs::exists(file)) {
      std::cout << "File does not exist: " << file << std::endl;
      return true; // this file does not exist
    }
    std::cout << "File was not deleted: " << file << std::endl;
    return false;
  };
  for (const auto& file_id : group_id_groups[gid]) {
    const auto &file = id_to_file[file_id];
    if (!file_deleted(file) && !parent_dir_deleted(file)){
      dupes.push_back(file);
    }
  }
  if (dupes.size() <= 1) {
    return "";
  }
  file["dupes"] = dupes;
  return file.dump();
}

int main() {
  std::cout << "Loading... ";
  parser exec;
  std::cout << "OK" << std::endl;

  beej::server server(10001);
  server.on_line([&](const std::string& file) -> std::pair<bool, std::string> {
    auto filename = file;
    if (filename[filename.size() - 1] == '\n') {
      filename = filename.substr(0, filename.size() - 1);
    }
    if (filename[filename.size() - 1] == '\r') {
      filename = filename.substr(0, filename.size() - 1);
    }
    const fs::path sandbox{filename};
    if (fs::is_regular_file(fs::status(sandbox))) {
      std::cout << "lookup regular file " << filename << std::endl;
      return std::make_pair(true, exec.query(filename.substr(0, filename.size())) + "\n"s);
    } else if (fs::is_directory(fs::status(sandbox))) {
      std::cout << "lookup directory " << filename << std::endl;
      std::stringstream ss;
      for (auto const& dir_entry : fs::directory_iterator{sandbox}) {
        ss << exec.query(dir_entry.path().string()) << '\n';
      }
      return std::make_pair(true, ss.str() + "\n"s);
    }
    return std::make_pair(false, "ERROR\n");
  });
  server.run();
}
