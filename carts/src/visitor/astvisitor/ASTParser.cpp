/*
    Author: Knud Andersen
    Date: 07/29/24
    ===============================================================================================================
    Description: Tools for parsing clang's Abstract Syntax Tree (AST) in the
   context of gathering OpenMP Metadata |
    ===============================================================================================================

*/

#include "ASTParser.h"
/*
  FUNCTION:
      size_t ASTsplit(const std::string& txt, std::vector<std::string>& strs,
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
size_t ASTsplit(const std::string &txt, std::vector<std::string> &strs,
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
      void DeleteRootListHelper(AST* curr)

  DESCRIPTION:
      Recursively deletes nodes in the AST starting from the current node
  `curr`. PARAMETERS: curr - The current AST node to start deletion from.
*/
void DeleteRootListHelper(AST *curr) {
  /*
      Recursive helper for destroying root list.
  */
  if (curr == NULL) {
    return;
  }
  for (size_t i = 0; i < curr->children.size(); i++) {
    DeleteRootListHelper(curr->children[i]);
  }
  delete curr;
}
/*
  FUNCTION:
      void ASTDeleteRootList(std::vector<AST*>& root_list)

  DESCRIPTION:
      Deletes all AST nodes in the root list by calling `DeleteRootListHelper`
  on each node. PARAMETERS: root_list - A vector of AST root nodes to be
  deleted.
*/

void ASTDeleteRootList(std::vector<AST *> &root_list) {
  for (size_t i = 0; i < root_list.size(); i++) {
    DeleteRootListHelper(root_list[i]);
  }
}
/*
  FUNCTION:
      size_t ASTGetIndentationLevel(const std::string& line)

  DESCRIPTION:
      Computes the indentation level of a given line by counting leading
  whitespace and special characters (` `, ```, `|`). PARAMETERS: line - The
  input line for which to calculate the indentation level. RETURNS: The number
  of leading whitespace and special characters in the line.
*/
size_t ASTGetIndentationLevel(const std::string &line) {
  size_t count = 0;
  for (size_t i = 0; i < line.size(); i++) {
    char current_char = line[i];
    if (isspace(current_char) || current_char == '`' || current_char == '|') {
      count++;
    } else {
      break;
    }
  }
  return count;
}
/*
  FUNCTION:
      size_t LineIsDirective(const std::string& line)

  DESCRIPTION:
      Checks if a line from the AST file is a valid directive by looking for it
  in the `VALID_DIRECTIVES` set. PARAMETERS: line - The line to be checked.
  RETURNS:
      1 if the line is a valid directive, 0 otherwise.
*/
size_t LineIsDirective(const std::string &line) {
  /*
      Returns if the line from AST file is a valid directive by checking if
     it's in VALID_DIRECTIVES set.
  */
  for (auto &dir : VALID_DIRECTIVES) {
    if (line.find(dir) != std::string::npos) {
      return 1;
    }
  }
  return 0;
}
/*
  FUNCTION:
      AST* InitASTNode(const std::string& directive_line)

  DESCRIPTION:
      Initializes a new AST node based on the provided directive line, setting
  its level and data. PARAMETERS: directive_line - The line from the AST file to
  initialize the node with. RETURNS: A pointer to the newly created AST node.
*/
AST *InitASTNode(const std::string &directive_line) {
  /*
      Initializes new AST node given line from ast.txt.
  */
  AST *new_node = new AST();
  new_node->level = ASTGetIndentationLevel(directive_line);
  std::string fixed = directive_line.substr(
      new_node->level, directive_line.size() - new_node->level);
  new_node->data = fixed;
  return new_node;
}

/*
  CLASS:
      CustomPriorityQueueComparator

  DESCRIPTION:
      Custom comparator for a priority queue used in the `ProcessDirective`
  function to order nodes by their indentation level.
*/

class CustomPriorityQueueComparator {
  /*
      Custom PriorityQueue comparator for ProcessDirective function.
  */
public:
  bool operator()(const std::pair<size_t, AST *> &a,
                  const std::pair<size_t, AST *> &b) {
    return a.first < b.first;
  }
};
/*
  FUNCTION:
      AST* ProcessDirective(std::streampos& curr_pos, std::ifstream& ast_file)

  DESCRIPTION:
      Processes a single OpenMP directive from the AST file and constructs its
  AST representation. Handles nested directives and maintains the hierarchy by
  using a priority queue.

  PARAMETERS:
      curr_pos - The current position in the file from which to start
  processing. This is updated to the position after the directive has been
  processed. ast_file - The input file stream containing the AST data.

  RETURNS:
      A pointer to the root AST node of the processed directive.

  ALGORITHM:
      1. Read the first line of the directive. If it fails to read, output an
  error message and terminate.
      2. Determine the indentation level of the directive line to set as the
  root level.
      3. Initialize a new AST node for the root of the directive, setting its
  level and data.
      4. Create a priority queue to manage the nodes based on their indentation
  levels, starting with the root node.
      5. Enter a loop to read subsequent lines from the file:
         - If a directive is encountered, set the skip flag and skip lines until
  the indentation level decreases. (We will handle this directive at a later
  point, see nested vector.)
         - If the indentation level of the current line is less than or equal to
  the root's level, end the processing and return the root.
         - Otherwise, initialize a new AST node for the current line, and manage
  its position in the priority queue.
      6. Ensure that nodes are properly linked as children of their parent nodes
  based on their indentation levels.
      7. Return the root AST node once processing is complete.
*/
AST *ProcessDirective(std::streampos &curr_pos, std::ifstream &ast_file) {
  /*
      Converts a single directive to its AST tree component.
  */
  std::string directive_line;
  std::string line;
  size_t root_indent = -1;
  size_t skip_indent = -1;
  int skip = 0;
  if (!std::getline(ast_file, directive_line)) {
    fprintf(stderr,
            "invalid position passed into ProcessDirective, check line %d\n",
            __LINE__);
    exit(1);
  }
  root_indent = ASTGetIndentationLevel(directive_line);
  AST *root = InitASTNode(directive_line);
  const std::pair<size_t, AST *> pair = std::make_pair(root->level, root);
  std::priority_queue<std::pair<size_t, AST *>,
                      std::vector<std::pair<size_t, AST *>>,
                      CustomPriorityQueueComparator>
      pq;
  pq.push(pair);
  while (std::getline(ast_file, line)) {
    if (skip) {
      if (ASTGetIndentationLevel(line) <= skip_indent) {
        skip = 0;
      } else {
        continue;
      }
    }
    if (LineIsDirective(line)) {
      skip_indent = ASTGetIndentationLevel(line);
      skip = 1;
      continue;
    } else if (ASTGetIndentationLevel(line) <= root_indent) {
      return root;
    }
    AST *curr = InitASTNode(line);
    const std::pair<size_t, AST *> curr_pair =
        std::make_pair(curr->level, curr);
    while (pq.size() > 0 && pq.top().first >= curr->level) {
      pq.pop();
    }
    if (pq.size() == 0) {
      break;
    } else {
      pq.top().second->children.push_back(curr);
      pq.push(curr_pair);
    }
  }
  return root;
}
/*
  FUNCTION:
      void PushDirectives(std::vector<AST*>& root_list, std::ifstream& ast_file)

  DESCRIPTION:
      Processes the AST file to find and record all OpenMP directives and their
  children, adding them to the root list. PARAMETERS: root_list - A vector to
  store the root nodes of processed AST trees. ast_file - The ifstream object
  for reading the AST file.
*/
void PushDirectives(std::vector<AST *> &root_list, std::ifstream &ast_file) {
  /*
      Goes through the AST file, if a line is a OpenMP directive, then we
     record all of its children. If another OpenMP directive is encountered
     while we are processing one ...
                                      ... we skip it and mark it to be
     handeled later by going to the next line with same indentation level.
  */
  std::vector<std::streampos> directive_positions;
  std::queue<std::streampos> nested;
  std::string line;
  std::streampos prev_pos = ast_file.tellg();
  while (std::getline(ast_file, line)) {
    if (LineIsDirective(line)) {
      directive_positions.push_back(prev_pos);
    }
    prev_pos = ast_file.tellg();
  }
  for (size_t i = 0; i < directive_positions.size(); i++) {
    if (ast_file.eof()) {
      ast_file.close();
      ast_file.open(INTERNAL_AST_PATH);
    }
    std::streampos curr_pos = directive_positions[i];
    ast_file.seekg(curr_pos);
    AST *root = ProcessDirective(curr_pos, ast_file);
    root_list.push_back(root);
  }
  return;
}
/*
  FUNCTION:
      std::vector<AST*> ASTGetRootList(std::ifstream& ast_file)

  DESCRIPTION:
      Retrieves a list of root AST nodes from the AST file by calling
  `PushDirectives`. PARAMETERS: ast_file - The ifstream object for reading the
  AST file. RETURNS: A vector of pointers to the root AST nodes.
*/
std::vector<AST *> ASTGetRootList(std::ifstream &ast_file) {
  std::vector<AST *> root_list;
  PushDirectives(root_list, ast_file);
  return root_list;
}
/*
  FUNCTION:
      void PrintRootListHelper(AST* curr, int level)

  DESCRIPTION:
      Recursively prints the AST starting from the current node `curr` with
  indentation representing the level in the hierarchy. PARAMETERS: curr - The
  current AST node to print. level - The current level of indentation for
  printing.
*/
void PrintRootListHelper(AST *curr, int level) {
  /*
      Recursive helper for printing root list.
  */
  if (curr == NULL) {
    return;
  }
  for (int i = 0; i < level; i++) {
    fprintf(stderr, "-");
  }
  fprintf(stderr, "'%s'\n", curr->data.c_str());
  for (size_t i = 0; i < curr->children.size(); i++) {
    PrintRootListHelper(curr->children[i], level + 3);
  }
  return;
}
/*
  FUNCTION:
      void ASTPrintRootList(std::vector<AST*>& root_list)

  DESCRIPTION:
      Prints all AST nodes in the root list by calling `PrintRootListHelper` for
  each node. PARAMETERS: root_list - A vector of AST root nodes to be printed.
*/
void ASTPrintRootList(std::vector<AST *> &root_list) {
  for (size_t i = 0; i < root_list.size(); i++) {
    AST *curr = root_list[i];
    PrintRootListHelper(curr, 0);
  }
}
/*
  FUNCTION:
      int ASTGetRootLine(AST* root)

  DESCRIPTION:
      Extracts and returns the line number associated with the root AST node by
  parsing its data. PARAMETERS: root - The root AST node from which to extract
  the line number. RETURNS: The line number if found, -1 otherwise.
*/
int ASTGetRootLine(AST *root) {
  int line_val;
  size_t line_off = root->data.find("line:");
  if (line_off == std::string::npos) {
    return -1;
  }
  int scan_status = sscanf(root->data.c_str() + line_off + 5, "%d", &line_val);
  if (!scan_status) {
    return -1;
  }
  return line_val;
}
/*
  FUNCTION:
      void GetRootClausesHelper(AST* curr, std::vector<AST*>& clauses)

  DESCRIPTION:
      Recursively collects AST nodes that represent OpenMP clauses from the
  subtree rooted at `curr`. PARAMETERS: curr - The current AST node to start
  collecting clauses from. clauses - A vector to store the collected OpenMP
  clauses.
*/
void GetRootClausesHelper(AST *curr, std::vector<AST *> &clauses) {
  /*
      Recursive helper for getting root list clauses.
  */
  if (curr == NULL) {
    return;
  }
  if (curr->data.find("-OMP") != std::string::npos) {
    clauses.push_back(curr);
  }
  for (size_t i = 0; i < curr->children.size(); i++) {
    GetRootClausesHelper(curr->children[i], clauses);
  }
  return;
}
/*
  FUNCTION:
      std::vector<AST*> ASTGetRootClauses(AST* root)

  DESCRIPTION:
      Retrieves all OpenMP clauses from the root node's children by calling
  `GetRootClausesHelper`. PARAMETERS: root - The root AST node to get clauses
  from. RETURNS: A vector of AST nodes representing the OpenMP clauses.
*/
std::vector<AST *> ASTGetRootClauses(AST *root) {
  std::vector<AST *> clauses;
  for (size_t i = 0; i < root->children.size(); i++) {
    GetRootClausesHelper(root->children[i], clauses);
  }

  return clauses;
}
/*
  FUNCTION:
      std::map<std::string, std::pair<int, int>>
  ASTGetVariableVector(std::ifstream& ast_file)

  DESCRIPTION:
      Extracts and returns a map of variable addresses to their line and column
  numbers from the AST file. PARAMETERS: ast_file - The ifstream object for
  reading the AST file. RETURNS: A map where each key is a variable address and
  the value is a pair of line and column numbers.
*/
std::map<std::string, std::pair<int, int>>
ASTGetVariableVector(std::ifstream &ast_file) {
  if (ast_file.eof()) {
    ast_file.close();
    ast_file.open(INTERNAL_AST_PATH);
  }
  std::map<std::string, std::pair<int, int>> outmap;
  ast_file.seekg(0);
  std::string line;
  int last_line_num = -1;
  int last_col_num = -1;
  while (std::getline(ast_file, line)) {
    if (line.find("-DeclStmt") != std::string::npos) {
      int tmp = ASTGetLineNum(line);
      if (tmp >= 0) {
        last_line_num = tmp;
      }
      size_t indentation_level = ASTGetIndentationLevel(line);
      std::string child;
      std::streampos prev;
      while (std::getline(ast_file, child)) {
        if (child.find("-VarDecl") != std::string::npos) {
          size_t indentation_level = ASTGetIndentationLevel(child);
          std::string child_f =
              child.substr(indentation_level, child.size() - indentation_level);
          std::vector<std::string> child_split;
          ASTsplit(child_f, child_split, ' ');
          std::string child_addr = child_split[1];
          last_col_num = ASTGetColumnNum(child);
          outmap.insert({child_addr, {last_line_num, last_col_num}});
        }
        size_t child_indentation_level = ASTGetIndentationLevel(child);
        if (child_indentation_level == indentation_level) {
          ast_file.seekg(prev);
          break;
        }
        prev = ast_file.tellg();
      }
    }
  }
  return outmap;
}
/*
  FUNCTION:
      int ASTGetColumnNum(std::string& str)

  DESCRIPTION:
      Extracts the column number from a string containing variable information.
  PARAMETERS:
      str - The string to extract the column number from.
  RETURNS:
      The column number if found, -1 otherwise.
*/
int ASTGetColumnNum(std::string &str) {
  size_t pos = 0;
  size_t last_pos = 0;
  size_t tmp = 0;
  while (1) {
    pos = str.find("col:", pos);
    if (pos == std::string::npos) {
      pos = last_pos;
      break;
    }
    last_pos = pos;
    pos += 4;
  }
  int ret;
  int scan_result = sscanf(str.c_str() + pos + 4, "%d", &ret);
  if (scan_result == 1) {
    return ret;
  }
  return -1;
}
/*
  FUNCTION:
      int ASTGetLineNum(std::string& str)

  DESCRIPTION:
      Extracts the line number from a string containing variable information.
  PARAMETERS:
      str - The string to extract the line number from.
  RETURNS:
      The line number if found, -1 otherwise.
*/
int ASTGetLineNum(std::string &str) {
  size_t line_off = str.find("line:");
  if (line_off == std::string::npos) {
    return -1;
  }
  int ret;
  int scan_result = sscanf(str.c_str() + line_off + 5, "%d", &ret);
  if (scan_result) {
    return ret;
  }
  return -1;
}
/*
  FUNCTION:
      int ASTGetVariableColumn(const std::string& variable_address,
  std::map<std::string, std::pair<int, int>>& ast_variables)

  DESCRIPTION:
      Retrieves the column number of a variable identified by `variable_address`
  from the map `ast_variables`. PARAMETERS: variable_address - The address of
  the variable. ast_variables - A map of variable addresses to their line and
  column numbers. RETURNS: The column number if the variable address is found,
  -1 otherwise.
*/
int ASTGetVariableColumn(
    const std::string &variable_address,
    std::map<std::string, std::pair<int, int>> &ast_variables) {
  if (ast_variables.count(variable_address)) {
    return ast_variables[variable_address].second;
  }
  return -1;
}
/*
  FUNCTION:
      int ASTGetVariableLine(const std::string& variable_address,
  std::map<std::string, std::pair<int, int>>& ast_variables)

  DESCRIPTION:
      Retrieves the line number of a variable identified by `variable_address`
  from the map `ast_variables`. PARAMETERS: variable_address - The address of
  the variable. ast_variables - A map of variable addresses to their line and
  column numbers. RETURNS: The line number if the variable address is found, -1
  otherwise.
*/
int ASTGetVariableLine(
    const std::string &variable_address,
    std::map<std::string, std::pair<int, int>> &ast_variables) {
  if (ast_variables.count(variable_address)) {
    return ast_variables[variable_address].first;
  }
  return -1;
}
/*
  FUNCTION:
      int ASTGetNumThreads(AST* num_threads_clause)

  DESCRIPTION:
      Retrieves the number of threads specified in the `num_threads_clause` AST
  node by parsing its data. PARAMETERS: num_threads_clause - The AST node
  containing the number of threads information. RETURNS: The number of threads
  if found, -1 otherwise.
*/
int ASTGetNumThreads(AST *num_threads_clause) {
  if (num_threads_clause->children.size() != 1) {
    return -1;
  }
  AST *child = num_threads_clause->children[0];
  std::vector<std::string> string_split;
  ASTsplit(child->data, string_split, ' ');
  std::string str_num_threads = string_split[string_split.size() - 1];
  int num_threads;
  num_threads = std::stoi(str_num_threads);
  return num_threads;
}