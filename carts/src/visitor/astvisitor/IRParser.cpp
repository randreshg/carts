/*
    Author: Knud Andersen
    Date: 07/29/24
    =====================================================================================================================
    Description: Tools for parsing clang's Intermediate Representation (IR) in
   the context of gathering OpenMP Metadata |
    =====================================================================================================================

*/

#include "IRParser.h"

/*
  FUNCTION:
      size_t IRsplit(const std::string& txt, std::vector<std::string>& strs,
  char ch)

  DESCRIPTION:
      Splits a string `txt` into a vector of strings `strs` using a specified
  delimiter `ch`. Handles quoted segments to avoid splitting within quotes.
  PARAMETERS:
      txt - The input string to be split.
      strs - A vector to store the resulting substrings.
      ch - The delimiter character used for splitting the string.
  RETURNS:
      The number of substrings added to `strs`.
*/

size_t IRsplit(const std::string &txt, std::vector<std::string> &strs,
               char ch) {
  int quote_flag = 0;
  std::string current = "";

  for (size_t i = 0; i < txt.length(); i++) {
    char c = txt[i];

    if (c == '\'') {
      if (quote_flag == 0) {
        quote_flag = 1;
      } else {
        quote_flag = 0;
        strs.push_back(current);
        current.clear();
      }
    } else if (c == ch && quote_flag == 0) {
      // if delimiter and not inside quoted segment, push current to strs
      if (!current.empty()) {
        strs.push_back(current);
        current.clear();
      }
    } else {
      if (c != '\n')
        current += c;
    }
  }
  if (!current.empty()) {
    strs.push_back(current);
  }
  return strs.size();
}

/*
  FUNCTION:
      ssize_t MoveFilePtrUntilChar(std::ifstream& file, char c, int reset_flag)

  DESCRIPTION:
      Moves the file pointer in `file` until a line starting with the character
  `c` is found. If `reset_flag` is non-zero, the file pointer is reset to the
  beginning before searching. PARAMETERS: file - The ifstream object
  representing the file to read from. c - The character to search for at the
  start of a line. reset_flag - Flag indicating whether to reset the file
  pointer to the beginning before searching. RETURNS: 1 if the line is found, -1
  if not found.
*/

ssize_t MoveFilePtrUntilChar(std::ifstream &file, char c, int reset_flag) {
  /*
      Reads from file until line starts with specified character.
      If reset_flag is specified, file pointer is moved back to the start
     before the search.
  */
  if (reset_flag) {
    file.seekg(std::ios_base::beg);
  }
  std::string line;
  std::streampos prev_pos = 0;
  while (std::getline(file, line)) {
    if (line[0] == c) {
      file.seekg(prev_pos);
      return 1;
    }
    prev_pos = file.tellg();
  }
  return -1;
}
/*
  FUNCTION:
      std::vector<int> GetDirectiveLines(std::ifstream& ir_file)

  DESCRIPTION:
      Scans the top section of the IR file to extract and return a vector of
  line numbers where OpenMP directives are located. The IR file format is
  assumed to be LLVM 16. PARAMETERS: ir_file - The ifstream object for reading
  the IR file. RETURNS: A vector of integers representing line numbers of OpenMP
  directives.
*/
std::vector<int> GetDirectiveLines(std::ifstream &ir_file) {
  /*
      Scans the top section of IR file and returns a vector of OMP directive
     line numbers. Follows LLVM 16 format -- please examine and tweak the
     magic numbers if behavior is undesired.
  */
  std::vector<int> directive_lines;
  ssize_t mv_stat = MoveFilePtrUntilChar(ir_file, '@', 1);
  if (mv_stat == -1) {
    fprintf(stderr, "ir parser: Issue finding desired section, check line %d\n",
            __LINE__);
    exit(1);
  }
  std::string line;
  while (std::getline(ir_file, line) && line[0] != '\0') {
    std::vector<std::string> line_split;
    IRsplit(line, line_split, ' ');
    if (line_split.size() != 11) {
      continue;
    }
    std::string target_string = line_split[line_split.size() - 3];
    std::vector<std::string> target_string_split;
    IRsplit(target_string, target_string_split, ';');
    if (target_string_split.size() != 6) {
      continue;
    }
    std::string string_line = target_string_split[3];

    int int_line = stoi(string_line);
    directive_lines.push_back(int_line);
  }
  std::set<int> s;
  unsigned size = directive_lines.size();
  for (unsigned i = 0; i < size; ++i)
    s.insert(directive_lines[i]);
  directive_lines.assign(s.begin(), s.end());
  return directive_lines;
}
/*
  FUNCTION:
      std::vector<int> ParseDebugIDs(const std::string& dbg_line)

  DESCRIPTION:
      Parses a line of IR debug information to extract all debug IDs formatted
  as !%d and stores them in a vector. The first element in the vector is the
  primary ID of the line. PARAMETERS: dbg_line - The line of debug information
  containing debug IDs. RETURNS: A vector of integers representing the parsed
  debug IDs.
*/
std::vector<int> ParseDebugIDs(const std::string &dbg_line) {
  /*
      Given a line of IR debug info, parses all of the !%d and stores it in a
     vector. Naturally, the first element of the vector represents the ID of
     that line, i.e. the 10 in "!10 = abcdefg"
  */
  std::vector<int> out;
  const char *s = dbg_line.c_str();
  for (size_t i = 0; i < dbg_line.size(); i++) {
    int cur_int;
    if (s[i] == '!' && i != dbg_line.size() - 1) {
      int scan_status = sscanf(s + i + 1, "%d", &cur_int);
      if (scan_status) {
        out.push_back(cur_int);
      }
    }
  }
  return out;
}
/*
  FUNCTION:
      std::map<int, std::vector<int>> DebugInfoToTree(std::ifstream& ir_file)

  DESCRIPTION:
      Reads debug information from the IR file and creates a map of debug IDs to
  associated IDs found on the same lines. This map represents the hierarchical
  structure of debug information. PARAMETERS: ir_file - The ifstream object for
  reading the IR file. RETURNS: A map where each key is a debug ID and the value
  is a vector of associated IDs.
*/
std::map<int, std::vector<int>> DebugInfoToTree(std::ifstream &ir_file) {
  if (ir_file.eof()) {
    ir_file.close();
    ir_file.open(INTERNAL_IR_PATH);
  }
  std::map<int, std::vector<int>> debug_map;
  MoveFilePtrUntilChar(ir_file, '!', 1);
  MoveFilePtrUntilChar(ir_file, '\0', 0);
  MoveFilePtrUntilChar(ir_file, '!', 0);
  std::string line;
  while (std::getline(ir_file, line) && line[0] != '\0') {
    std::vector<int> dbg_ids = ParseDebugIDs(line);
    int id = dbg_ids[0];
    dbg_ids.erase(dbg_ids.begin(), dbg_ids.begin() + 1);
    debug_map.insert({id, dbg_ids});
  }
  return debug_map;
}
/*
  FUNCTION:
      std::map<int, int> DirectiveLineToID(std::ifstream& ir_file, std::map<int,
  std::vector<int>>& debug_info, std::vector<int>& directive_lines)

  DESCRIPTION:
      Matches the previously gathered OpenMP directive lines to their
  corresponding debug IDs by scanning through the debug section of the IR file.
  PARAMETERS:
      ir_file - The ifstream object for reading the IR file.
      debug_info - A map of debug IDs to associated IDs.
      directive_lines - A vector of line numbers where OpenMP directives are
  located. RETURNS: A map where each key is a debug ID and the value is the
  corresponding line number of the directive.
*/
std::map<int, int>
DirectiveLineToID(std::ifstream &ir_file,
                  std::map<int, std::vector<int>> &debug_info,
                  std::vector<int> &directive_lines) {
  /*
      Goes through debugging section, matches previously gathered OMPDirective
     lines to debug ID.
  */
  if (ir_file.eof()) {
    ir_file.close();
    ir_file.open(INTERNAL_IR_PATH);
  }
  MoveFilePtrUntilChar(ir_file, '!', 1);
  MoveFilePtrUntilChar(ir_file, '\0', 0);
  MoveFilePtrUntilChar(ir_file, '!', 0);
  std::string line;
  std::map<int, int> out;
  std::set<int> s(directive_lines.begin(), directive_lines.end());
  while (std::getline(ir_file, line)) {
    if (line.find("DISubprogram") != std::string::npos ||
        line.find("DILexicalBlock") != std::string::npos) {

      // Subprogram or Lexical Block
      size_t line_off = line.find("line: ");
      if (line_off == std::string::npos) {
        continue;
      }

      int line_val;
      int scan_line_res = sscanf(line.c_str() + 6 + line_off, "%d", &line_val);
      if (!scan_line_res) {
        continue;
      }

      if (!s.count(line_val)) {
        continue;
      }

      int line_id;
      int scan_id_res = sscanf(line.c_str() + 1, "%d", &line_id);
      if (!scan_id_res) {
        continue;
      }

      if (out.count(line_id)) {
        fprintf(stderr, "MAJOR ERROR OCCURED, CONTACT ME! ir parser line %d\n",
                __LINE__);
        exit(1);
      } else {
        out.insert({line_id, line_val});
      }
    }
  }
  return out;
}
/*
  FUNCTION:
      IRMap IterateVariables(std::ifstream& ir_file, std::map<int,
  std::vector<int>>& debug_info, std::vector<int>& dir_lines, std::map<int,
  int>& dir_line_to_id)

  DESCRIPTION:
      Iterates through the debug information and assigns variables to OpenMP
  directives based on the gathered debug information and directive lines.
  PARAMETERS:
      ir_file - The ifstream object for reading the IR file.
      debug_info - A map of debug IDs to associated IDs.
      dir_lines - A vector of line numbers where OpenMP directives are located.
      dir_line_to_id - A map of directive line numbers to their corresponding
  debug IDs. RETURNS: A map where each key is a directive line number and the
  value is a vector of variable names associated with that directive.
*/
IRMap IterateVariables(std::ifstream &ir_file,
                       std::map<int, std::vector<int>> &debug_info,
                       std::vector<int> &dir_lines,
                       std::map<int, int> &dir_line_to_id) {
  /*
      Goes through debug info and assigns variables to OpenMP directives.
  */
  if (ir_file.eof()) {
    ir_file.close();
    ir_file.open(INTERNAL_IR_PATH);
  }
  MoveFilePtrUntilChar(ir_file, '!', 1);
  MoveFilePtrUntilChar(ir_file, '\0', 0);
  MoveFilePtrUntilChar(ir_file, '!', 0);
  std::string line;
  IRMap outvec;
  for (auto &x : dir_lines) {
    std::vector<std::string> v;
    outvec.insert({x, v});
  }
  while (std::getline(ir_file, line)) {
    if (line.find("DILocalVariable") != std::string::npos &&
        line.find("name: ") != std::string::npos) {
      // Variable here
      int id_val;
      int scan_status = sscanf(line.c_str() + 1, "%d", &id_val);
      if (!scan_status) {
        continue;
      }
      size_t name_off = line.find("name: ");
      size_t name_len = 0;
      while (line[name_off + name_len + 7] != ' ') {
        name_len++;
      }
      std::string name = line.substr(name_off + 7, name_len - 2);
      if (name[0] == '.') {
        continue;
      }
      std::vector<int> search = debug_info[id_val];
      for (size_t i = 0; i < search.size(); i++) {
      }
      int inserted = 0;
      for (size_t i = 0; i < search.size(); i++) {
        if (inserted) {
          break;
        }
        if (dir_line_to_id.count(search[i])) {
          outvec[dir_line_to_id[search[i]]].push_back(name);
        }
      }
    }
  }
  return outvec;
}
/*
  FUNCTION:
      IRMap IRGetVariables(std::ifstream& ir_file)

  DESCRIPTION:
      A wrapper function that orchestrates the major API calls to extract and
  map variables from the IR file to OpenMP directives. PARAMETERS: ir_file - The
  ifstream object for reading the IR file. RETURNS: An IRMap containing
  variables mapped to their respective OpenMP directives.
*/
IRMap IRGetVariables(std::ifstream &ir_file) {
  /*
      Wrapper that puts the major API calls together.
  */
  std::vector<int> directive_lines = GetDirectiveLines(ir_file);
  std::map<int, std::vector<int>> debug_info = DebugInfoToTree(ir_file);
  std::map<int, int> debug_id_to_directive_line =
      DirectiveLineToID(ir_file, debug_info, directive_lines);
  return IterateVariables(ir_file, debug_info, directive_lines,
                          debug_id_to_directive_line);
}
