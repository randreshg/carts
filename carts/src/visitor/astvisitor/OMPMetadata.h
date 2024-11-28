/*
    Author: Knud Andersen
    Date: 07/29/24
    ==================================================================================================
    Description: Tools that compare analyses from IR and AST parsers in order to
   get OpenMP metadata |
    ==================================================================================================

*/
#include <map>
#include <string>
#include <vector>
/*
    Metadata node that represents a single variable.
    Contains all the relevant information we are searching for.
*/
struct OMPVariable {
  std::string clause;
  std::string name;
  std::string type;
  std::string addr;
  void *parent;
  int line;
  int column;
};

/*
    Metadata node that represents an OpenMP directive.
*/
struct OMPDirective {
  std::string directive_name;
  size_t line;
  ssize_t num_threads;
  std::vector<struct OMPVariable *> variables;
};
static const std::string GLOBAL_IR_PATH = "/people/ande409/tmp/data/output.ll";
static const std::string GLOBAL_AST_PATH = "/people/ande409/tmp/data/AST.txt";

/*
    Combines information from AST and IR parsers and returns a vector of
   directive nodes.
*/
std::vector<OMPDirective *> GetMetadata(const std::string &ast_filename,
                                        const std::string &ir_filename);

/*
    Prints metadata/directive list and all of its variables.
*/
void OMPPrintMetadata(const std::vector<OMPDirective *> &directive_list);

/*
    Frees the metadata/directive list from dynamic memory.
*/
void OMPDestroyMetadata(std::vector<OMPDirective *> &directive_list);

/*
    Opens a file from filename.
*/
std::ifstream OpenFile(const std::string &filename);
