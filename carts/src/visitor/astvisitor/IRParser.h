
//============================================================================//
//                                IRParser.h
// - Description: Tools for parsing clang's Intermediate Representation (IR) in
//   the context of gathering OpenMP Metadata
// - Author: Knud Andersen (07/29/24)
// - Modified by: Rafael Andres Herrera Guaitero (11/07/24)
//============================================================================//

#include <cctype>
#include <fstream>
#include <map>
#include <string>
#include <vector>

///  Path to built AST file with command such as:
///   clang -S -fopenmp -emit-llvm -g3 -O3 -fno-discard-value-names example.c
///         -o data/output.ll
const std::string INTERNAL_IR_PATH = "/people/ande409/tmp/data/output.ll";

/// Type definition -- keys are OpenMP directive lines, and values are vectors
/// of variable names that can be traced to that directive.
typedef std::map<int, std::vector<std::string>> IRMap;

/// Parses IR file, returns map. Keys are OpenMP directive lines. Values are
/// vectors of variable names that belong to that OpenMP directive.
IRMap IRGetVariables(std::ifstream &ir_file);

/// Given a string, split it up by delimiter and store it in the vector. Note:
/// counts whatever is in single quotes as one element, so ' asdasd asdas' is
/// one element even though delimiter may occur within it.
size_t IRsplit(const std::string &txt, std::vector<std::string> &strs, char ch);
