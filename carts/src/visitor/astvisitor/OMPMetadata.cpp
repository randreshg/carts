/*
    Author: Knud Andersen
    Date: 07/29/24
    ==================================================================================================
    Description: Tools that compare analyses from IR and AST parsers in order to
   get OpenMP metadata |
    ==================================================================================================

*/

#include "OMPMetadata.h"
#include "ASTParser.h"
#include "IRParser.h"
/*
  FUNCTION:
      std:string GetClauseName(AST* clause)

  DESCRIPTION:
      Given an AST* node that represents an OpenMP clause (i.e. contains
  "-OMP"), return its name. PARAMETERS: clause - an AST node that represents an
  OpenMP clause e.g. '-OMPSharedClause <other data here>' RETURNS: A string with
  the name of the OpenMP clause, e.g. '-OMPSharedClause' in the example above.
*/
std::string GetClauseName(AST *clause) {
  std::string clause_name;
  int clause_len = 0;
  while (clause->data[clause_len++] != ' ') {
    continue;
  }
  clause_name = clause->data.substr(0, clause_len - 1);
  return clause_name;
}
/*
  FUNCTION:
      OMPVariable *InitNullOMPVariable(std::string &clause_name)
  DESCRIPTION:
      Given a string name of an OpenMP clause, initializes an empty OpenMP
  struct with that name. PARAMETERS: clause_name - The name of an OpenMP clause.
  RETURNS:
      An allocated OMPVariable* that stores the clause name.

*/
OMPVariable *InitNullOMPVariable(std::string &clause_name) {
  OMPVariable *new_variable = new OMPVariable();
  new_variable->type = "<NULL>";
  new_variable->name = "<NULL>";
  new_variable->addr = "<NULL>";
  new_variable->clause = clause_name;
  return new_variable;
}
/*
  FUNCTION:
      OMPVariable *InitOMPVariable(std::vector<std::string> &line_split,
  std::string &clause_name) DESCRIPTION: Allocates a new OMPVariable* with
  information parsed from the given line split and clause name. PARAMETERS:
      line_split - A vector of strings parsed from a line of data.
      clause_name - The name of the OpenMP clause.
  RETURNS:
      An allocated OMPVariable* that stores the parsed data and clause name.
*/
OMPVariable *InitOMPVariable(std::vector<std::string> &line_split,
                             std::string &clause_name) {
  OMPVariable *new_variable = new OMPVariable();
  new_variable->type = line_split[line_split.size() - 1];
  new_variable->name = line_split[line_split.size() - 2];
  new_variable->addr = line_split[line_split.size() - 3];
  new_variable->clause = clause_name;
  return new_variable;
}
/*
  FUNCTION:
      void GetDirectiveListHandleTaskDirectives(std::vector<AST *> &clauses,
  OMPDirective *new_directive) DESCRIPTION: Processes OpenMP task directives by
  iterating over clauses and variables to populate the new_directive with
  task-related variables. PARAMETERS: clauses - A vector of AST* nodes
  representing OpenMP clauses. new_directive - A pointer to an OMPDirective that
  will be populated with task-related variables.
*/
void GetDirectiveListHandleTaskDirectives(std::vector<AST *> &clauses,
                                          OMPDirective *new_directive) {
  /*
      GetDirectiveLine -- handles OpenMP tasking becuase there is not enough
     OpenMP IR data on it.
  */
  for (auto &clause : clauses) {
    std::string clause_name = GetClauseName(clause);
    for (auto &variable : clause->children) {
      if (variable->data.find("-DeclRefExpr") != std::string::npos) {
        std::vector<std::string> line_split;
        ASTsplit(variable->data, line_split, ' ');
        OMPVariable *new_variable = InitOMPVariable(line_split, clause_name);
        new_directive->variables.push_back(new_variable);
      }
    }
  }
}
/*
  FUNCTION:
      void GetDirectiveListHandleRegularDirectives(std::vector<AST *> &clauses,
  OMPDirective *new_directive, const std::pair<const int,
  std::vector<std::string>> &ir_variable_entry) DESCRIPTION: Handles regular
  OpenMP directives by parsing clauses and matching variables against IR
  entries, and populates new_directive accordingly. PARAMETERS: clauses - A
  vector of AST* nodes representing OpenMP clauses. new_directive - A pointer to
  an OMPDirective that will be populated with variable data. ir_variable_entry -
  A pair containing an integer key and a vector of IR variable names for
  matching.
*/
void GetDirectiveListHandleRegularDirectives(
    std::vector<AST *> &clauses, OMPDirective *new_directive,
    const std::pair<const int, std::vector<std::string>> &ir_variable_entry) {
  /*
      GetDirectiveLine -- handles regular directives.
  */
  int null_added = 0;
  for (auto &clause : clauses) {
    for (auto &variable : clause->children) {
      if (null_added) {
        null_added = 0;
      }
      for (auto &ir_var_name : ir_variable_entry.second) {
        std::vector<std::string> line_split;
        ASTsplit(variable->data, line_split, ' ');
        std::string clause_name = GetClauseName(clause);
        if (line_split.size() == 9) {
          std::string variable_name = line_split[line_split.size() - 2];
          if (variable->data.find("-DeclRefExpr") != std::string::npos &&
              variable_name.find(ir_var_name) != std::string::npos &&
              ir_var_name.find(variable_name) != std::string::npos) {
            // Match
            OMPVariable *new_variable =
                InitOMPVariable(line_split, clause_name);
            new_directive->variables.push_back(new_variable);
          }
        } else {
          if (clause_name == "-OMPNum_threadsClause") {
            new_directive->num_threads = ASTGetNumThreads(clause);
          } else {
            OMPVariable *new_variable = InitNullOMPVariable(clause_name);
            new_directive->variables.push_back(new_variable);
            null_added = 1;
          }
        }
        if (null_added) {
          break;
        }
      }
    }
  }
}
/*
  FUNCTION:
      OMPDirective *InitOMPDirective(int &root_line, std::string
  &directive_name) DESCRIPTION: Initializes a new OMPDirective with the given
  root line and directive name. PARAMETERS: root_line - The line number where
  the directive is found. directive_name - The name of the OpenMP directive.
  RETURNS:
      An allocated OMPDirective* initialized with the provided line and name.
*/
OMPDirective *InitOMPDirective(int &root_line, std::string &directive_name) {
  OMPDirective *new_directive = new OMPDirective();
  new_directive->line = root_line;
  new_directive->directive_name = directive_name;
  return new_directive;
}
/*
  FUNCTION:
      std::vector<OMPDirective *> GetDirectiveListWithoutLines(const IRMap
  &ir_variables, const std::vector<AST *> &root_list) DESCRIPTION: Processes the
  provided IR variables and AST root list to generate a vector of OMPDirective*
  without line numbers. PARAMETERS: ir_variables - A map of IR variables used
  for matching with AST data. root_list - A vector of AST* nodes representing
  root elements in the AST. RETURNS: A vector of OMPDirective* with metadata but
  without line numbers, which are added later.
*/
std::vector<OMPDirective *>
GetDirectiveListWithoutLines(const IRMap &ir_variables,
                             const std::vector<AST *> &root_list) {
  /*
      Outputs a vector of processed directive metadata nodes.
      Initially without line numbers, which are added later.
  */
  std::vector<OMPDirective *> directives;
  for (auto &pair : ir_variables) {
    for (auto &curr_root : root_list) {
      int root_line = ASTGetRootLine(curr_root);
      if (root_line == pair.first) {
        std::string directive_name = GetClauseName(curr_root);
        OMPDirective *new_directive =
            InitOMPDirective(root_line, directive_name);
        std::vector<AST *> clauses = ASTGetRootClauses(curr_root);
        if (directive_name == "-OMPTaskDirective" ||
            directive_name == "-OMPTaskwaitDirective") {
          GetDirectiveListHandleTaskDirectives(clauses, new_directive);
        } else {
          GetDirectiveListHandleRegularDirectives(clauses, new_directive, pair);
        }
        directives.push_back(new_directive);
      }
    }
  }
  return directives;
}
/*
  FUNCTION:
      void OMPPrintMetadata(const std::vector<OMPDirective *> &directive_list)
  DESCRIPTION:
      Prints the metadata of the given vector of OMPDirective* to stderr for
  debugging purposes. PARAMETERS: directive_list - A vector of OMPDirective* to
  be printed.
*/
void OMPPrintMetadata(const std::vector<OMPDirective *> &directive_list) {
  for (auto &x : directive_list) {
    fprintf(stderr, "Directive '%s'\n", x->directive_name.c_str());
    fprintf(stderr, "\tLine: %zu\n", x->line);
    fprintf(stderr, "\tNum Threads: %zu\n", x->num_threads);
    for (auto &y : x->variables) {
      fprintf(stderr, "\tVariables:\n");
      fprintf(stderr, "\t\tName: '%s'\n", y->name.c_str());
      fprintf(stderr, "\t\tClause: '%s'\n", y->clause.c_str());
      fprintf(stderr, "\t\tAddress: '%s'\n", y->addr.c_str());
      fprintf(stderr, "\t\tLine: %d\n", y->line);
      fprintf(stderr, "\t\tColumn: %d\n", y->column);
      fprintf(stderr, "\t\tType: '%s'\n", y->type.c_str());
    }
    fprintf(stderr, "\n");
  }
}
/*
  FUNCTION:
      void OMPDestroyMetadata(std::vector<OMPDirective *> &directive_list)
  DESCRIPTION:
      Cleans up and deletes all OMPDirective* and their associated OMPVariable*
  in the given vector to free memory. PARAMETERS: directive_list - A vector of
  OMPDirective* whose memory will be freed.
*/
void OMPDestroyMetadata(std::vector<OMPDirective *> &directive_list) {
  for (size_t i = 0; i < directive_list.size(); i++) {
    for (size_t j = 0; j < directive_list[i]->variables.size(); j++) {
      delete directive_list[i]->variables[j];
    }
    delete directive_list[i];
  }
}
/*
  FUNCTION:
      void UpdateVariableLines(std::ifstream &ast_file, std::vector<OMPDirective
  *> &directive_list) DESCRIPTION: Updates the line numbers and column
  information for each OMPVariable* in the directive list based on the AST
  file's variable data. PARAMETERS: ast_file - An ifstream object for reading
  the AST file. directive_list - A vector of OMPDirective* that will be updated
  with line and column information.
*/
void UpdateVariableLines(std::ifstream &ast_file,
                         std::vector<OMPDirective *> &directive_list) {
  /*
      Scans through initial directive list, and updates line numbers given
     stored addresses.
  */
  std::map<std::string, std::pair<int, int>> ast_variables =
      ASTGetVariableVector(ast_file);
  for (auto &directive : directive_list) {
    for (auto &variable : directive->variables) {
      std::string variable_address = variable->addr;
      variable->line = ASTGetVariableLine(variable_address, ast_variables);
      variable->column = ASTGetVariableColumn(variable_address, ast_variables);
    }
  }
  return;
}
/*
  FUNCTION:
      std::vector<OMPDirective *> GetDirectiveList(const IRMap &ir_variables,
  const std::vector<AST *> &root_list, std::ifstream &ast_file) DESCRIPTION:
      Returns the fully formatted list of OMPDirective* with all necessary
  metadata, including updated line numbers. PARAMETERS: ir_variables - A map of
  IR variables used for directive processing. root_list - A vector of AST* nodes
  representing the root elements of the AST. ast_file - An ifstream object for
  reading the AST file to update line numbers. RETURNS: A vector of fully
  populated OMPDirective*.
*/
std::vector<OMPDirective *>
GetDirectiveList(const IRMap &ir_variables, const std::vector<AST *> &root_list,
                 std::ifstream &ast_file) {
  /*
      Returns the fully formatted directive list.
  */
  std::vector<OMPDirective *> directive_list =
      GetDirectiveListWithoutLines(ir_variables, root_list);
  UpdateVariableLines(ast_file, directive_list);
  return directive_list;
}
/*
  FUNCTION:
      std::ifstream OpenFile(const std::string &filename)
  DESCRIPTION:
      Opens a file with the given filename and returns an ifstream object for
  reading. PARAMETERS: filename - The path to the file to be opened. RETURNS: An
  ifstream object for the specified file.
*/
std::ifstream OpenFile(const std::string &filename) {
  std::ifstream outfile(filename);
  if (!outfile.is_open()) {
    std::cerr << "ERROR! Could not open AST file, double check the path: "
              << filename << std::endl;
  }
  return outfile;
}
/*
  FUNCTION:
      std::vector<OMPDirective *> GetMetadata(const std::string &ast_filename,
  const std::string &ir_filename)

  DESCRIPTION:
      Retrieves and processes metadata from the specified AST and IR files.
  Opens the files, extracts and parses OpenMP-related directives and their
  metadata, and associates variables with their parent directives. PARAMETERS:
      ast_filename - The path to the AST file containing the abstract syntax
  tree data. ir_filename - The path to the IR file containing intermediate
  representation variables. RETURNS: A vector of pointers to OMPDirective* that
  contains the metadata parsed from the AST and IR files, with variables linked
  to their respective directives.
*/
std::vector<OMPDirective *> GetMetadata(const std::string &ast_filename,
                                        const std::string &ir_filename) {
  std::ifstream ast_file = OpenFile(ast_filename);
  std::ifstream ir_file = OpenFile(ir_filename);
  const IRMap ir_variables = IRGetVariables(ir_file);
  std::vector<AST *> root_list = ASTGetRootList(ast_file);
  std::vector<OMPDirective *> directive_list =
      GetDirectiveList(ir_variables, root_list, ast_file);
  for (size_t i = 0; i < directive_list.size(); i++) {
    OMPDirective *directive_ptr = directive_list[i];
    for (size_t j = 0; j < directive_list[i]->variables.size(); j++) {
      directive_list[i]->variables[j]->parent = (void *)directive_ptr;
    }
  }
  ASTDeleteRootList(root_list);

  return directive_list;
}

/* int main(void)
{
    std::ifstream ir_file = OpenFile(GLOBAL_IR_PATH);
    std::ifstream ast_file = OpenFile(GLOBAL_AST_PATH);
    IRMap ir_variables = IRGetVariables(ir_file);
    std::vector<AST*> root_list = ASTGetRootList(ast_file);
    std::vector<OMPDirective*> directive_list
        = GetDirectiveList(ir_variables, root_list, ast_file);

    OMPPrintMetadata(directive_list);
    OMPDestroyMetadata(directive_list);
    ASTDeleteRootList(root_list);
    return 0;
} */
