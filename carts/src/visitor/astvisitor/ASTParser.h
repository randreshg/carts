
//============================================================================//
//                                ASTParser.h
// - Description: Tools for parsing clang's Abstract Syntax Tree (AST) in the
//   context of gathering OpenMP Metadata
// - Author: Knud Andersen (07/29/24)
// - Modified by: Rafael Andres Herrera Guaitero (11/07/24)
//============================================================================//

#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <stdio.h>
#include <string>
#include <utility>
#include <vector>

///  Node for a line in AST.txt file. Built like a tree.
struct AST {
  std::string Data;
  size_t Level;
  std::vector<struct AST *> Children;
};

/// Path to built AST file with command such as:
///  clang -c -O3 -fopenmp -Xclang -ast-dump example.c &> Data/AST.txt
const std::string INTERNAL_AST_PATH = "/people/ande409/tmp/Data/AST.txt";
/// String flags for directive names in AST.txt file.
static const std::string parallel_directive = "-OMPParallelDirective";
static const std::string parallel_for_directive = "-OMPParallelForDirective";
static const std::string for_directive = "-OMPForDirective";
static const std::string task_directive = "-OMPTaskDirective";
static const std::string task_wait_directive = "-OMPTaskwaitDirective";
static const std::string atomic_directive = "-OMPAtomicDirective";
static const std::string critical_directive = "-OMPCriticalDirective";
static const std::string ordered_directive = "-OMPOrderedDirective";

const std::set<std::string> VALID_DIRECTIVES = {
    parallel_directive, parallel_for_directive, for_directive,
    task_directive,     task_wait_directive,    atomic_directive,
    critical_directive, ordered_directive};

/// Deletes AST tree from memory
void ASTDeleteRootList(std::vector<AST *> &root_list);

/// Returns vector of OpenMP directive roots (connected components) of generated
/// AST tree.
std::vector<AST *> ASTGetRootList(std::ifstream &ast_file);

/// Prints root list entirely.
void ASTPrintRootList(std::vector<AST *> &root_list);

/// Given a root, return the OpenMP clauses that are its Children.
std::vector<AST *> ASTGetRootClauses(AST *root);

/// Given a line of AST.txt file, return its indentation Level, effectively
/// counting the leading whitespace, '|', and '`'.
size_t ASTGetIndentationLevel(const std::string &line);

/// Given an AST root, return its line number.
int ASTGetRootLine(AST *root);

/// Given an AST number of threads clause, return the number of threads value.
int ASTGetNumThreads(AST *num_threads_clause);

/// Given a string, split it up by delimiter and store it in the vector.
/// Note: counts whatever is in single quotes as one element, so ' asdasd asdas'
/// is one element even though delimiter may occur within it.
size_t ASTsplit(const std::string &txt, std::vector<std::string> &strs,
                char ch);

/// Given a variable address, return the line number it matches to.
int ASTGetVariableLine(
    const std::string &variable_address,
    std::map<std::string, std::pair<int, int>> &ast_variables);
int ASTGetColumnLine(const std::string &variable_address,
                     std::map<std::string, std::pair<int, int>> &ast_variables);
/// Given a string representing a line in AST.txt file, return its line number.
int ASTGetLineNum(std::string &str);

/// Scans variable initializations in AST file. Keys are addresses provided in
/// the AST, and values are corresponding line number.
std::map<std::string, std::pair<int, int>>
ASTGetVariableVector(std::ifstream &ast_file);

int ASTGetColumnNum(std::string &str);
int ASTGetVariableColumn(
    const std::string &variable_address,
    std::map<std::string, std::pair<int, int>> &ast_variables);